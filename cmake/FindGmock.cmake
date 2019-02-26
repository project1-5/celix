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


# - Try to find Gmock
# Once done this will define
#  GMOCK_FOUND - System has GMock
#  GMOCK_INCLUDE_DIRS - The GMock include directories
#  GMOCK_LIBRARIES - The libraries needed to use GMock
#  GMOCK_DEFINITIONS - Compiler switches required for using Jansson

set(GMOCK_INCLUDE_DIR "/usr/include/gmock/")

find_library(GMOCK_LIBRARY NAMES libgmock.a
             PATHS /usr/lib /usr/local/lib )

set(GMOCK_LIBRARIES ${GMOCK_LIBRARY} )
set(GMOCK_INCLUDE_DIRS ${GMOCK_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set GMOCK_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Gmock  DEFAULT_MSG
                                  GMOCK_LIBRARY GMOCK_INCLUDE_DIR)

mark_as_advanced(GMOCK_INCLUDE_DIR GMOCK_LIBRARY )
