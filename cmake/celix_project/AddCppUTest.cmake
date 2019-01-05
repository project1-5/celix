# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

include(ExternalProject)
ExternalProject_Add(
        cpputest_project
        GIT_REPOSITORY https://github.com/cpputest/cpputest.git
        GIT_TAG v3.8
        UPDATE_DISCONNECTED TRUE
        PREFIX ${CMAKE_BINARY_DIR}/cpputest
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/cpputest
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/cpputest/include)

set(CPPUTEST_LIBRARY cpputest::cpputest)
set(CPPUTEST_EXT_LIBRARY cpputest::ext)
set(CPPUTEST_INCLUDE_DIR ${binary_dir}/cpputest)

add_library(cpputest::cpputest IMPORTED STATIC GLOBAL)
add_dependencies(cpputest::cpputest cpputest_project)
set_target_properties(cpputest::cpputest PROPERTIES
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/cpputest/lib/libCppUTest.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/cpputest/include"
)

add_library(cpputest::ext IMPORTED STATIC GLOBAL)
add_dependencies(cpputest::ext cpputest_project)
set_target_properties(cpputest::ext PROPERTIES
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/cpputest/lib/libCppUTestExt.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/cpputest/include"
)
