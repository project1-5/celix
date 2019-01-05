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
        libzip_project
        GIT_REPOSITORY https://github.com/nih-at/libzip.git
        GIT_TAG rel-1-5-1
        UPDATE_DISCONNECTED TRUE
        PREFIX ${CMAKE_BINARY_DIR}/libzip
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libzip -DCMAKE_C_FLAGS=-fPIC -DBUILD_SHARED_LIBS=OFF -DENABLE_COMMONCRYPTO=OFF -DENABLE_GNUTLS=OFF -DENABLE_OPENSSL=OFF -Wno-dev
)

file(MAKE_DIRECTORY ${source_dir}/libzip/include)

add_library(libzip::libzip IMPORTED STATIC GLOBAL)
add_dependencies(libzip::libzip libzip_project)
set_target_properties(libzip::libzip PROPERTIES
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/libzip/lib/libzip.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/libzip/include"
)