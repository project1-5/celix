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

find_package(glog REQUIRED)

#TODO rename to celix::shell && celix::shell_api

add_library(shell_api_cxx INTERFACE)
target_include_directories(shell_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include/celix/shell>
)

add_library(celix_shell_cxx STATIC
        src/ShellActivator.cc
        include/celix/IShell.h)
target_include_directories(celix_shell_cxx PRIVATE src)
target_include_directories(celix_shell_cxx PUBLIC include)
target_link_libraries(celix_shell_cxx PRIVATE glog::glog celix_framework_cxx)

#if (ENABLE_TESTING)
#    add_subdirectory(gtest)
#endif ()