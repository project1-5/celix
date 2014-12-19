/**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 */
/*
 * dm_component_impl.c
 *
 *  \date       9 Oct 2014
 *  \author     <a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright  Apache License, Version 2.0
 */

#include <stdarg.h>
#include <stdlib.h>

#include "dm_component_impl.h"

struct dm_executor {
    pthread_t runningThread;
    bool runningThreadSet;
    linked_list_pt workQueue;

    pthread_mutex_t mutex;
};

struct dm_executor_task {
    dm_component_pt component;
    void (*command)(void *command, void *data);
    void *data;
};

struct dm_handle_event_type {
	dm_service_dependency_pt dependency;
	dm_event_pt event;
	dm_event_pt newEvent;
};

typedef struct dm_handle_event_type *dm_handle_event_type_pt;

static celix_status_t executor_runTasks(dm_executor_pt executor, pthread_t currentThread);
static celix_status_t executor_execute(dm_executor_pt executor);
static celix_status_t executor_executeTask(dm_executor_pt executor, dm_component_pt component, void (*command), void *data);
static celix_status_t executor_schedule(dm_executor_pt executor, dm_component_pt component, void (*command), void *data);
static celix_status_t executor_create(dm_component_pt component, dm_executor_pt *executor);
static celix_status_t executor_destroy(dm_executor_pt *executor);

static celix_status_t component_destroyComponent(dm_component_pt component);
static celix_status_t component_invokeRemoveRequiredDependencies(dm_component_pt component);
static celix_status_t component_invokeRemoveInstanceBoundDependencies(dm_component_pt component);
static celix_status_t component_invokeRemoveOptionalDependencies(dm_component_pt component);
static celix_status_t component_registerService(dm_component_pt component);
static celix_status_t component_unregisterService(dm_component_pt component);
static celix_status_t component_invokeAddOptionalDependencies(dm_component_pt component);
static celix_status_t component_invokeAddRequiredInstanceBoundDependencies(dm_component_pt component);
static celix_status_t component_invokeAddRequiredDependencies(dm_component_pt component);
static celix_status_t component_allInstanceBoundAvailable(dm_component_pt component, bool *available);
static celix_status_t component_allRequiredAvailable(dm_component_pt component, bool *available);
static celix_status_t component_performTransition(dm_component_pt component, dm_component_state_pt oldState, dm_component_state_pt newState, bool *transition);
static celix_status_t component_calculateNewState(dm_component_pt component, dm_component_state_pt currentState, dm_component_state_pt *newState);
static celix_status_t component_handleChange(dm_component_pt component);
static celix_status_t component_startDependencies(dm_component_pt component, array_list_pt dependencies);

static celix_status_t component_addTask(dm_component_pt component, array_list_pt dependencies);
static celix_status_t component_startTask(dm_component_pt component, void* data);
static celix_status_t component_stopTask(dm_component_pt component, void* data);
static celix_status_t component_removeTask(dm_component_pt component, dm_service_dependency_pt dependency);
static celix_status_t component_handleEventTask(dm_component_pt component, dm_handle_event_type_pt data);

static celix_status_t component_handleAdded(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event);
static celix_status_t component_handleChanged(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event);
static celix_status_t component_handleRemoved(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event);
static celix_status_t component_handleSwapped(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event, dm_event_pt newEvent);

celix_status_t component_create(bundle_context_pt context, dm_dependency_manager_pt manager, dm_component_pt *component) {
    celix_status_t status = CELIX_SUCCESS;

    *component = malloc(sizeof(**component));
    if (!*component) {
        status = CELIX_ENOMEM;
    } else {
        (*component)->context = context;
        (*component)->manager = manager;

        (*component)->state = DM_CMP_STATE_INACTIVE;

        (*component)->callbackInit = NULL;
        (*component)->callbackStart = NULL;
        (*component)->callbackStop = NULL;
        (*component)->callbackDestroy = NULL;

        (*component)->implementation = NULL;

        arrayList_create(&(*component)->dependencies);

        pthread_mutex_init(&(*component)->mutex, NULL);

        (*component)->isStarted = false;
        (*component)->active = false;
        (*component)->dependencyEvents = hashMap_create(NULL, NULL, NULL, NULL);

        (*component)->executor = NULL;
        executor_create(*component, &(*component)->executor);
    }

    return status;
}

celix_status_t component_destroy(dm_component_pt component_ptr) {
	celix_status_t status = CELIX_SUCCESS;

	if (!component_ptr) {
		status = CELIX_ILLEGAL_ARGUMENT;
	}

	if (status == CELIX_SUCCESS) {
		// #TODO destroy dependencies?
		executor_destroy(&component_ptr->executor);
		hashMap_destroy(component_ptr->dependencyEvents, false, false);
		pthread_mutex_destroy(&component_ptr->mutex);
		arrayList_destroy(component_ptr->dependencies);

		free(component_ptr);
	}

	return status;
}

celix_status_t component_addServiceDependency(dm_component_pt component, ...) {
    celix_status_t status = CELIX_SUCCESS;

    array_list_pt dependenciesList = NULL;
    arrayList_create(&dependenciesList);

    va_list dependencies;
    va_start(dependencies, component);
    dm_service_dependency_pt dependency = va_arg(dependencies, dm_service_dependency_pt);
    while (dependency != NULL) {
        arrayList_add(dependenciesList, dependency);


        dependency = va_arg(dependencies, dm_service_dependency_pt);
    }

    va_end(dependencies);

	executor_executeTask(component->executor, component, component_addTask, dependenciesList);

    return status;
}


celix_status_t component_addTask(dm_component_pt component, array_list_pt dependencies) {
    celix_status_t status = CELIX_SUCCESS;

    array_list_pt bounds = NULL;
    arrayList_create(&bounds);
    for (int i = 0; i < arrayList_size(dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(dependencies, i);

        pthread_mutex_lock(&component->mutex);
        array_list_pt events = NULL;
        arrayList_create(&events);
        hashMap_put(component->dependencyEvents, dependency, events);

        arrayList_add(component->dependencies, dependency);
        pthread_mutex_unlock(&component->mutex);

        serviceDependency_setComponent(dependency, component);
        if (!(component->state == DM_CMP_STATE_INACTIVE)) {
            serviceDependency_setInstanceBound(dependency, true);
            arrayList_add(bounds, dependency);
        }
        component_startDependencies(component, bounds);
        component_handleChange(component);
    }

    return status;
}

celix_status_t component_removeServiceDependency(dm_component_pt component, dm_service_dependency_pt dependency) {
    celix_status_t status = CELIX_SUCCESS;

    executor_executeTask(component->executor, component, component_removeTask, dependency);

    return status;
}

celix_status_t component_removeTask(dm_component_pt component, dm_service_dependency_pt dependency) {
    celix_status_t status = CELIX_SUCCESS;

    pthread_mutex_lock(&component->mutex);
    arrayList_removeElement(component->dependencies, dependency);
    pthread_mutex_unlock(&component->mutex);

    if (!(component->state == DM_CMP_STATE_INACTIVE)) {
        serviceDependency_stop(dependency);
    }

    pthread_mutex_lock(&component->mutex);
    array_list_pt events = hashMap_remove(component->dependencyEvents, dependency);
    for (int i = arrayList_size(events); i > 0; i--) {
    	dm_event_pt event = arrayList_remove(events, i - 1);
    	event_destroy(&event);
    }
    arrayList_destroy(events);
    pthread_mutex_unlock(&component->mutex);

    component_handleChange(component);

    return status;
}

celix_status_t component_start(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    component->active = true;
    executor_executeTask(component->executor, component, component_startTask, NULL);

    return status;
}

celix_status_t component_startTask(dm_component_pt component, void* data) {
    celix_status_t status = CELIX_SUCCESS;

    component->isStarted = true;
    component_handleChange(component);

    return status;
}

celix_status_t component_stop(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    component->active = false;
    executor_executeTask(component->executor, component, component_stopTask, NULL);

    return status;
}

celix_status_t component_stopTask(dm_component_pt component, void* data) {
    celix_status_t status = CELIX_SUCCESS;

    component->isStarted = false;
    component_handleChange(component);
    component->active = false;

    return status;
}

celix_status_t component_setInterface(dm_component_pt component, char *serviceName, properties_pt properties) {
    celix_status_t status = CELIX_SUCCESS;

    if (component->active) {
        return CELIX_ILLEGAL_STATE;
    } else {
        component->serviceName = serviceName;
        component->properties = properties;
    }

    return status;
}

celix_status_t component_handleEvent(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event) {
	celix_status_t status = CELIX_SUCCESS;

	dm_handle_event_type_pt data = calloc(1, sizeof(*data));
	data->dependency = dependency;
	data->event = event;
	data->newEvent = NULL;

	status = executor_executeTask(component->executor, component, component_handleEventTask, data);

	return status;
}

celix_status_t component_handleEventTask(dm_component_pt component, dm_handle_event_type_pt data) {
	celix_status_t status = CELIX_SUCCESS;

	switch (data->event->event_type) {
		case DM_EVENT_ADDED:
			component_handleAdded(component,data->dependency, data->event);
			break;
		case DM_EVENT_CHANGED:
			component_handleChanged(component,data->dependency, data->event);
			break;
		case DM_EVENT_REMOVED:
			component_handleRemoved(component,data->dependency, data->event);
			break;
		case DM_EVENT_SWAPPED:
			component_handleSwapped(component,data->dependency, data->event, data->newEvent);
			break;
		default:
			break;
	}

	return status;
}

celix_status_t component_handleAdded(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event) {
    celix_status_t status = CELIX_SUCCESS;

    array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
    arrayList_add(events, event);
    serviceDependency_setAvailable(dependency, true);

    switch (component->state) {
        case DM_CMP_STATE_WAITING_FOR_REQUIRED: {
            bool required = false;
            serviceDependency_isRequired(dependency, &required);
            if (required) {
                component_handleChange(component);
            }
            break;
        }
        case DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED: {
            bool instanceBound = false;
            serviceDependency_isInstanceBound(dependency, &instanceBound);
            if (!instanceBound) {
                bool required = false;
                serviceDependency_isRequired(dependency, &required);
                if (required) {
                    serviceDependency_invokeAdd(dependency, event);
                }
                // updateInstance
            } else {
                bool required = false;
                serviceDependency_isRequired(dependency, &required);
                if (required) {
                    component_handleChange(component);
                }
            }
            break;
        }
        case DM_CMP_STATE_TRACKING_OPTIONAL:
            serviceDependency_invokeAdd(dependency, event);
            // updateInstance
            break;
        default:
            break;
    }

    return status;
}

celix_status_t component_handleChanged(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event) {
    celix_status_t status = CELIX_SUCCESS;

    array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
    arrayList_removeElement(events, event);
    arrayList_add(events, event);

    switch (component->state) {
        case DM_CMP_STATE_TRACKING_OPTIONAL:
            serviceDependency_invokeChange(dependency, event);
            // updateInstance
            break;
        case DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED: {
            bool instanceBound = false;
            serviceDependency_isInstanceBound(dependency, &instanceBound);
            if (!instanceBound) {
                serviceDependency_invokeChange(dependency, event);
                // updateInstance
            }
            break;
        }
        default:
            break;
    }

    return status;
}

celix_status_t component_handleRemoved(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event) {
    celix_status_t status = CELIX_SUCCESS;

    array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
    int size = arrayList_size(events);
    if (arrayList_contains(events, event)) {
        size--;
    }
    serviceDependency_setAvailable(dependency, size > 0);
    component_handleChange(component);

    arrayList_removeElement(events, event);

    switch (component->state) {
        case DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED: {
            bool instanceBound = false;
            serviceDependency_isInstanceBound(dependency, &instanceBound);
            if (!instanceBound) {
                bool required = false;
                serviceDependency_isRequired(dependency, &required);
                if (required) {
                    serviceDependency_invokeRemove(dependency, event);
                }
                // updateInstance
            }
            break;
        }
        case DM_CMP_STATE_TRACKING_OPTIONAL:
            serviceDependency_invokeRemove(dependency, event);
            // updateInstance
            break;
        default:
            break;
    }

    return status;
}

celix_status_t component_handleSwapped(dm_component_pt component, dm_service_dependency_pt dependency, dm_event_pt event, dm_event_pt newEvent) {
    celix_status_t status = CELIX_SUCCESS;

    array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
    arrayList_removeElement(events, event);
    arrayList_add(events, newEvent);

    switch (component->state) {
        case DM_CMP_STATE_WAITING_FOR_REQUIRED:
            break;
        case DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED:
        {
            bool instanceBound = false;
            serviceDependency_isInstanceBound(dependency, &instanceBound);
            if (!instanceBound) {
                bool required = false;
                serviceDependency_isRequired(dependency, &required);
                if (required) {
                    serviceDependency_invokeSwap(dependency, event, newEvent);
                }
            }
            break;
        }
        case DM_CMP_STATE_TRACKING_OPTIONAL:
            serviceDependency_invokeSwap(dependency, event, newEvent);
            break;
        default:
            break;
    }

    return status;
}

celix_status_t component_startDependencies(dm_component_pt component, array_list_pt dependencies) {
    celix_status_t status = CELIX_SUCCESS;
    array_list_pt requiredDeps = NULL;
    arrayList_create(&requiredDeps);

    for (int i = 0; i < arrayList_size(dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(dependencies, i);
        bool required = false;
        serviceDependency_isRequired(dependency, &required);
        if (required) {
            arrayList_add(requiredDeps, dependency);
            continue;
        }

        serviceDependency_start(dependency);
    }

    for (int i = 0; i < arrayList_size(requiredDeps); i++) {
        dm_service_dependency_pt dependency = arrayList_get(requiredDeps, i);
        serviceDependency_start(dependency);
    }
    return status;
}

celix_status_t component_stopDependencies(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);
        serviceDependency_stop(dependency);
    }

    return status;
}

celix_status_t component_handleChange(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    dm_component_state_pt oldState;
    dm_component_state_pt newState;

    bool cont = false;
    do {
        oldState = component->state;
        component_calculateNewState(component, oldState, &newState);
        component->state = newState;
        component_performTransition(component, oldState, newState, &cont);
    } while (cont);

    return status;
}

celix_status_t component_calculateNewState(dm_component_pt component, dm_component_state_pt currentState, dm_component_state_pt *newState) {
    celix_status_t status = CELIX_SUCCESS;

    if (currentState == DM_CMP_STATE_INACTIVE) {
        if (component->isStarted) {
            *newState = DM_CMP_STATE_WAITING_FOR_REQUIRED;
            return status;
        }
    }
    if (currentState == DM_CMP_STATE_WAITING_FOR_REQUIRED) {
        if (!component->isStarted) {
            *newState = DM_CMP_STATE_INACTIVE;
            return status;
        }

        bool available = false;
        component_allRequiredAvailable(component, &available);

        if (available) {
            *newState = DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED;
            return status;
        }
    }
    if (currentState == DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED) {
        bool available = false;
        component_allRequiredAvailable(component, &available);

        if (component->isStarted && available) {
            bool instanceBoundAvailable = false;
            component_allInstanceBoundAvailable(component, &instanceBoundAvailable);

            if (instanceBoundAvailable) {
                *newState = DM_CMP_STATE_TRACKING_OPTIONAL;
                return status;
            }

            *newState = currentState;
            return status;
        }
        *newState = DM_CMP_STATE_WAITING_FOR_REQUIRED;
        return status;
    }
    if (currentState == DM_CMP_STATE_TRACKING_OPTIONAL) {
        bool instanceBoundAvailable = false;
        bool available = false;

        component_allInstanceBoundAvailable(component, &instanceBoundAvailable);
        component_allRequiredAvailable(component, &available);

        if (component->isStarted  && available && instanceBoundAvailable) {
            *newState = currentState;
            return status;
        }

        *newState = DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED;
        return status;
    }

    *newState = currentState;

    return status;
}

celix_status_t component_performTransition(dm_component_pt component, dm_component_state_pt oldState, dm_component_state_pt newState, bool *transition) {
    celix_status_t status = CELIX_SUCCESS;

    if (oldState == DM_CMP_STATE_INACTIVE && newState == DM_CMP_STATE_WAITING_FOR_REQUIRED) {
        component_startDependencies(component, component->dependencies);
//        #TODO Add listener support
//        notifyListeners(newState);
        *transition = true;
        return status;
    }

    if (oldState == DM_CMP_STATE_WAITING_FOR_REQUIRED && newState == DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED) {
        // #TODO Remove
//        component_instantiateComponent(component);
        component_invokeAddRequiredDependencies(component);
//        component_invokeAutoConfigDependencies(component);
        dm_component_state_pt stateBeforeCallingInit = component->state;
        if (component->callbackInit) {
        	component->callbackInit(component->implementation);
        }
        if (stateBeforeCallingInit == component->state) {
//            #TODO Add listener support
//            notifyListeners(newState); // init did not change current state, we can notify about this new state
        }
        *transition = true;
        return status;
    }

    if (oldState == DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED && newState == DM_CMP_STATE_TRACKING_OPTIONAL) {
        component_invokeAddRequiredInstanceBoundDependencies(component);
//        component_invokeAutoConfigInstanceBoundDependencies(component);
        if (component->callbackStart) {
        	component->callbackStart(component->implementation);
        }
        component_invokeAddOptionalDependencies(component);
        component_registerService(component);
//            #TODO Add listener support
//        notifyListeners(newState);
        return true;
    }

    if (oldState == DM_CMP_STATE_TRACKING_OPTIONAL && newState == DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED) {
        component_unregisterService(component);
        component_invokeRemoveOptionalDependencies(component);
        if (component->callbackStop) {
        	component->callbackStop(component->implementation);
        }
        component_invokeRemoveInstanceBoundDependencies(component);
//            #TODO Add listener support
//        notifyListeners(newState);
        *transition = true;
        return status;
    }

    if (oldState == DM_CMP_STATE_INSTANTIATED_AND_WAITING_FOR_REQUIRED && newState == DM_CMP_STATE_WAITING_FOR_REQUIRED) {
    	if (component->callbackDestroy) {
    		component->callbackDestroy(component->implementation);
    	}
        component_invokeRemoveRequiredDependencies(component);
//            #TODO Add listener support
//        notifyListeners(newState);
        bool needInstance = false;
//        component_someDependenciesNeedInstance(component, &needInstance);
        if (!needInstance) {
            component_destroyComponent(component);
        }
        *transition = true;
        return status;
    }

    if (oldState == DM_CMP_STATE_WAITING_FOR_REQUIRED && newState == DM_CMP_STATE_INACTIVE) {
        component_stopDependencies(component);
        component_destroyComponent(component);
//            #TODO Add listener support
//        notifyListeners(newState);
        *transition = true;
        return status;
    }

    *transition = false;
    return status;
}

celix_status_t component_allRequiredAvailable(dm_component_pt component, bool *available) {
    celix_status_t status = CELIX_SUCCESS;

    *available = true;
    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);
        bool required = false;
        bool instanceBound = false;

        serviceDependency_isRequired(dependency, &required);
        serviceDependency_isInstanceBound(dependency, &instanceBound);

        if (required && !instanceBound) {
            bool isAvailable = false;
            serviceDependency_isAvailable(dependency, &isAvailable);
            if (!isAvailable) {
                *available = false;
                break;
            }
        }
    }

    return status;
}

celix_status_t component_allInstanceBoundAvailable(dm_component_pt component, bool *available) {
    celix_status_t status = CELIX_SUCCESS;

    *available = true;
    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);
        bool required = false;
        bool instanceBound = false;

        serviceDependency_isRequired(dependency, &required);
        serviceDependency_isInstanceBound(dependency, &instanceBound);

        if (required && instanceBound) {
            bool isAvailable = false;
            serviceDependency_isAvailable(dependency, &isAvailable);
            if (!isAvailable) {
                *available = false;
                break;
            }
        }
    }

    return status;
}

celix_status_t component_invokeAddRequiredDependencies(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);

        bool required = false;
        bool instanceBound = false;

        serviceDependency_isRequired(dependency, &required);
        serviceDependency_isInstanceBound(dependency, &instanceBound);

        if (required && !instanceBound) {
            array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
            if (events) {
				for (int i = 0; i < arrayList_size(events); i++) {
					dm_event_pt event = arrayList_get(events, i);
					serviceDependency_invokeAdd(dependency, event);
				}
            }
        }
    }

    return status;
}

celix_status_t component_invokeAddRequiredInstanceBoundDependencies(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);

        bool required = false;
        bool instanceBound = false;

        serviceDependency_isRequired(dependency, &required);
        serviceDependency_isInstanceBound(dependency, &instanceBound);

        if (instanceBound && required) {
            array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
            if (events) {
				for (int i = 0; i < arrayList_size(events); i++) {
					dm_event_pt event = arrayList_get(events, i);
					serviceDependency_invokeAdd(dependency, event);
				}
            }
        }
    }

    return status;
}

celix_status_t component_invokeAddOptionalDependencies(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);

        bool required = false;

        serviceDependency_isRequired(dependency, &required);

        if (!required) {
            array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
            if (events) {
				for (int i = 0; i < arrayList_size(events); i++) {
					dm_event_pt event = arrayList_get(events, i);
					serviceDependency_invokeAdd(dependency, event);
				}
            }
        }
    }

    return status;
}

celix_status_t component_invokeRemoveOptionalDependencies(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);

        bool required = false;

        serviceDependency_isRequired(dependency, &required);

        if (!required) {
            array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
            if (events) {
				for (int i = 0; i < arrayList_size(events); i++) {
					dm_event_pt event = arrayList_get(events, i);
					serviceDependency_invokeRemove(dependency, event);
				}
            }
        }
    }

    return status;
}

celix_status_t component_invokeRemoveInstanceBoundDependencies(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);

        bool instanceBound = false;

        serviceDependency_isInstanceBound(dependency, &instanceBound);

        if (instanceBound) {
            array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
            if (events) {
				for (int i = 0; i < arrayList_size(events); i++) {
					dm_event_pt event = arrayList_get(events, i);
					serviceDependency_invokeRemove(dependency, event);
				}
            }
        }
    }

    return status;
}

celix_status_t component_invokeRemoveRequiredDependencies(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    for (int i = 0; i < arrayList_size(component->dependencies); i++) {
        dm_service_dependency_pt dependency = arrayList_get(component->dependencies, i);

        bool required = false;
        bool instanceBound = false;

        serviceDependency_isRequired(dependency, &required);
        serviceDependency_isInstanceBound(dependency, &instanceBound);

        if (!instanceBound && required) {
            array_list_pt events = hashMap_get(component->dependencyEvents, dependency);
            if (events) {
				for (int i = 0; i < arrayList_size(events); i++) {
					dm_event_pt event = arrayList_get(events, i);
					serviceDependency_invokeRemove(dependency, event);
				}
            }
        }
    }

    return status;
}

celix_status_t component_destroyComponent(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    component->implementation = NULL;

    return status;
}

celix_status_t component_registerService(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    if (component->context && component->serviceName) {
        bundleContext_registerService(component->context, component->serviceName, component->implementation, component->properties, &component->registration);
    }

    return status;
}

celix_status_t component_unregisterService(dm_component_pt component) {
    celix_status_t status = CELIX_SUCCESS;

    if (component->registration && component->serviceName) {
        serviceRegistration_unregister(component->registration);
        component->registration = NULL;
    }

    return status;
}

celix_status_t component_setCallbacks(dm_component_pt component, init_fpt init, start_fpt start, stop_fpt stop, destroy_fpt destroy) {
	if (component->active) {
		return CELIX_ILLEGAL_STATE;
	}
	component->callbackInit = init;
	component->callbackStart = start;
	component->callbackStop = stop;
	component->callbackDestroy = destroy;
	return CELIX_SUCCESS;
}

celix_status_t component_isAvailable(dm_component_pt component, bool *available) {
    *available = component->state == DM_CMP_STATE_TRACKING_OPTIONAL;
    return CELIX_SUCCESS;
}

celix_status_t component_setImplementation(dm_component_pt component, void *implementation) {
    component->implementation = implementation;
    return CELIX_SUCCESS;
}

celix_status_t component_getBundleContext(dm_component_pt component, bundle_context_pt *context) {
	celix_status_t status = CELIX_SUCCESS;

	if (!component) {
		status = CELIX_ILLEGAL_ARGUMENT;
	}

	if (status == CELIX_SUCCESS) {
		*context = component->context;
	}

	return status;
}


celix_status_t executor_create(dm_component_pt component, dm_executor_pt *executor) {
    celix_status_t status = CELIX_SUCCESS;

    *executor = malloc(sizeof(**executor));
    if (!*executor) {
        status = CELIX_ENOMEM;
    } else {
        linkedList_create(&(*executor)->workQueue);
        pthread_mutex_init(&(*executor)->mutex, NULL);
    }

    return status;
}

celix_status_t executor_destroy(dm_executor_pt *executor) {
	celix_status_t status = CELIX_SUCCESS;

	if (!*executor) {
		status = CELIX_ILLEGAL_ARGUMENT;
	}

	if (status == CELIX_SUCCESS) {
		pthread_mutex_destroy(&(*executor)->mutex);
		linkedList_destroy((*executor)->workQueue);

		free(*executor);
		*executor = NULL;
	}

	return status;
}

celix_status_t executor_schedule(dm_executor_pt executor, dm_component_pt component, void (*command), void *data) {
    celix_status_t status = CELIX_SUCCESS;

    struct dm_executor_task *task = NULL;
    task = malloc(sizeof(*task));
    if (!task) {
        status = CELIX_ENOMEM;
    } else {
        task->component = component;
        task->command = command;
        task->data = data;

        pthread_mutex_lock(&executor->mutex);
        linkedList_addLast(executor->workQueue, task);
        pthread_mutex_unlock(&executor->mutex);
    }

    return status;
}

celix_status_t executor_executeTask(dm_executor_pt executor, dm_component_pt component, void (*command), void *data) {
    celix_status_t status = CELIX_SUCCESS;

    // Check thread and executor thread, if the same, execute immediately.
//    bool execute = false;
//    pthread_mutex_lock(&executor->mutex);
//    pthread_t currentThread = pthread_self();
//    if (pthread_equal(executor->runningThread, currentThread)) {
//        execute = true;
//    }
//    pthread_mutex_unlock(&executor->mutex);

    // For now, just schedule.
    executor_schedule(executor, component, command, data);
    executor_execute(executor);

    return status;
}

celix_status_t executor_execute(dm_executor_pt executor) {
    celix_status_t status = CELIX_SUCCESS;
    pthread_t currentThread = pthread_self();

    pthread_mutex_lock(&executor->mutex);
    bool execute = false;
    if (!executor->runningThreadSet) {
        executor->runningThread = currentThread;
        executor->runningThreadSet = true;
        execute = true;
    }
    pthread_mutex_unlock(&executor->mutex);
    if (execute) {
        executor_runTasks(executor, currentThread);
    }

    return status;
}

celix_status_t executor_runTasks(dm_executor_pt executor, pthread_t currentThread) {
    celix_status_t status = CELIX_SUCCESS;
//    bool execute = false;

    do {
        struct dm_executor_task *entry = NULL;
        pthread_mutex_lock(&executor->mutex);
        while ((entry = linkedList_removeFirst(executor->workQueue)) != NULL) {
            pthread_mutex_unlock(&executor->mutex);

            entry->command(entry->component, entry->data);

            pthread_mutex_lock(&executor->mutex);
        }
        executor->runningThreadSet = false;
        pthread_mutex_unlock(&executor->mutex);

//        pthread_mutex_lock(&executor->mutex);
//        if (executor->runningThread == NULL) {
//            executor->runningThread = currentThread;
//            execute = true;
//        }
//        pthread_mutex_unlock(&executor->mutex);
    } while (!linkedList_isEmpty(executor->workQueue)); // && execute

    return status;
}
