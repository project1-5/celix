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
 * properties.h
 *
 *  Created on: Apr 27, 2010
 *      Author: dk489
 */

#ifndef PROPERTIES_H_
#define PROPERTIES_H_

#include "hash_map.h"

typedef HASH_MAP PROPERTIES;

PROPERTIES properties_create(void);
PROPERTIES properties_load(char * filename);
void properties_store(PROPERTIES properties, char * file, char * header);

char * properties_get(PROPERTIES properties, char * key);
char * properties_getWithDefault(PROPERTIES properties, char * key, char * defaultValue);
char * properties_set(PROPERTIES properties, char * key, char * value);

#endif /* PROPERTIES_H_ */