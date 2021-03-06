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
if (BUILD_DEPENDENCY_MANAGER_CXX)
    include_directories(
            ${PROJECT_SOURCE_DIR}/dependency_manager/public/include
            ${PROJECT_SOURCE_DIR}/dependency_manager_cxx/include
            ${PROJECT_SOURCE_DIR}/utils/public/include
            ${PROJECT_SOURCE_DIR}/shell/public/include
            ${PROJECT_SOURCE_DIR}/log_service/public/include
            api
    )

    add_subdirectory(phase1)
    add_subdirectory(phase2a)
    add_subdirectory(phase2b)
    add_subdirectory(phase3)
    add_subdirectory(phase3_locking)


    add_celix_container("dm_example_cxx"
        COPY 
        CXX
        BUNDLES
            shell
            shell_tui
            dm_shell
            log_service
            log_writer

            phase1_cxx
            phase2a_cxx
            phase2b_cxx
            phase3_cxx
            phase3_locking_cxx
        PROPERTIES
            LOGHELPER_ENABLE_STDOUT_FALLBACK=true
    )

    add_celix_docker(dm_exmpl_cxx
        CXX
        GROUP examples
        BUNDLES_DIR /usr/share/bundles
        WORKDIR /workspace
        BUNDLES
            shell
            shell_tui
            dm_shell
            log_service
            log_writer

            phase1_cxx
            phase2a_cxx
            phase2b_cxx
            phase3_cxx
            phase3_locking_cxx
        PROPERTIES
            LOGHELPER_ENABLE_STDOUT_FALLBACK=true
    )

endif ()
