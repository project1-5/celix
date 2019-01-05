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
        googlelog_project
        GIT_REPOSITORY https://github.com/google/glog.git
        GIT_TAG v0.3.5
        UPDATE_DISCONNECTED TRUE
        PREFIX ${CMAKE_BINARY_DIR}/glog
        CMAKE_ARGS -DWITH_GFLAGS=OFF -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/glog -DCMAKE_C_FLAGS=-w -DCMAKE_CXX_FLAGS=-w
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/glog/include)

add_library(glog::glog IMPORTED STATIC GLOBAL)
add_dependencies(glog::glog googlelog_project)
set_target_properties(glog::glog PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/glog/lib/libglog.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/glog/include"
)

