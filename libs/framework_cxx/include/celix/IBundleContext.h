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

#ifndef CXX_CELIX_IBUNDLECONTEXT_H
#define CXX_CELIX_IBUNDLECONTEXT_H

#include "celix/IBundle.h"

namespace celix {

    //TODO rename and drop I, because it not a complete interface (templates)
    class IBundleContext {
    public:
        virtual ~IBundleContext() = default;

        virtual std::shared_ptr<celix::IBundle> bundle() const noexcept = 0;

        template<typename I>
        celix::ServiceRegistration registerService(I &svc, celix::Properties props = {}) {
            return registry().registerService<I>(svc, std::move(props), bundle());
        }

        template<typename I>
        celix::ServiceRegistration registerService(std::shared_ptr<I> svc, celix::Properties props = {}) {
            return registry().registerService<I>(svc, std::move(props), bundle());
        }

        template<typename F>
        celix::ServiceRegistration registerFunctionService(std::string functionName, F&& function, celix::Properties props = {}) {
            return registry().registerFunctionService(std::move(functionName), std::forward<F>(function), std::move(props), bundle());
        }

        //TODO register C services

        virtual bool useBundle(long bndId, std::function<void(const celix::IBundle &bnd)> use) const noexcept = 0;
        virtual int useBundles(std::function<void(const celix::IBundle &bnd)> use, bool includeFrameworkBundle = true) const noexcept = 0;

        virtual bool stopBundle(long bndId) noexcept = 0;
        virtual bool startBundle(long bndId) noexcept = 0;
        //TODO install / uninstall bundles

        template<typename I>
        bool useService(std::function<void(I &svc)> use, const std::string &filter = "") const noexcept {
            return registry().useService<I>(std::move(use), filter, bundle());
        }

        template<typename I>
        bool useService(std::function<void(I &svc, const celix::Properties &props)> use, const std::string &filter = "") const noexcept {
            return registry().useService<I>(std::move(use), filter, bundle());
        }

        template<typename I>
        bool useService(std::function<void(I &svc, const celix::Properties &props, const celix::IResourceBundle &owner)> use, const std::string &filter = "") const noexcept {
            return registry().useService<I>(std::move(use), filter, bundle());
        }

        template<typename F>
        bool useFunctionService(const std::string &functionName, std::function<void(F &function)> use, const std::string &filter = "") const noexcept {
            return registry().useFunctionService<F>(functionName, std::move(use), filter, bundle());
        }

        template<typename F>
        bool useFunctionService(const std::string &functionName, std::function<void(F &function, const celix::Properties &props)> use, const std::string &filter = "") const noexcept {
            return registry().useFunctionService<F>(functionName, std::move(use), filter, bundle());
        }

        template<typename F>
        bool useFunctionService(const std::string &functionName, std::function<void(F &function, const celix::Properties &props, const celix::IResourceBundle &owner)> use, const std::string &filter = "") const noexcept {
            return registry().useFunctionService<F>(functionName, std::move(use), filter, bundle());
        }

        template<typename I>
        int useServices(std::function<void(I &svc)> use, const std::string &filter = "") const noexcept {
            return registry().useServices<I>(std::move(use), filter, bundle());
        }

        template<typename I>
        int useServices(std::function<void(I &svc, const celix::Properties &props)> use, const std::string &filter = "") const noexcept {
            return registry().useServices<I>(std::move(use), filter, bundle());
        }

        template<typename I>
        int useServices(std::function<void(I &svc, const celix::Properties &props, const celix::IResourceBundle &owner)> use, const std::string &filter = "") const noexcept {
            return registry().useServices<I>(std::move(use), filter, bundle());
        }

        template<typename F>
        int useFunctionServices(const std::string &functionName, std::function<void(F &function)> use, const std::string &filter = "") const noexcept {
            return registry().useFunctionServices<F>(functionName, std::move(use), filter, bundle());
        }

        template<typename F>
        int useFunctionServices(const std::string &functionName, std::function<void(F &function, const celix::Properties &props)> use, const std::string &filter = "") const noexcept {
            return registry().useFunctionServices<F>(functionName, std::move(use), filter, bundle());
        }

        template<typename F>
        int useFunctionServices(const std::string &functionName, std::function<void(F &function, const celix::Properties &props, const celix::IResourceBundle &owner)> use, const std::string &filter = "") const noexcept {
            return registry().useFunctionServices<F>(functionName, std::move(use), filter, bundle());
        }

        //TODO use C services

        template<typename I>
        celix::ServiceTracker trackServices(celix::ServiceTrackerOptions<I> options = {}) {
            return registry().trackServices<I>(std::move(options), bundle());
        }

        template<typename F>
        celix::ServiceTracker trackFunctionServices(std::string functionName, celix::ServiceTrackerOptions<F> options = {}) {
            return registry().trackFunctionServices<F>(functionName, std::move(options), bundle());
        }

        //TODO track C Services

        //TODO track trackers

        //TODO track c trackers


        virtual celix::ServiceRegistry& registry() const noexcept = 0;
        virtual celix::ServiceRegistry& cRegistry() const noexcept = 0;
    };
}

#endif //CXX_CELIX_IBUNDLECONTEXT_H
