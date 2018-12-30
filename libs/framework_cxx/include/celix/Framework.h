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

#ifndef CXX_CELIX_FRAMEWORK_H
#define CXX_CELIX_FRAMEWORK_H

#include <memory>

#include "celix/Constants.h"
#include "celix/ServiceRegistry.h"
#include "celix/IBundleContext.h"
#include "celix/IBundle.h"
#include "celix/IBundleActivator.h"

namespace celix {

    struct StaticBundleOptions {
        std::string name{};
        std::string group{};
        std::string version{};
        celix::Properties manifest{};

        std::function<celix::IBundleActivator*()> bundleActivatorFactory{};

        //TODO resources. poiting to bundle specific symbols which is linked zip file
        char * const resoucreZip = nullptr;
        size_t resourceZipLength = 0;
    };

    void registerStaticBundle(std::string symbolicName, const StaticBundleOptions &opts);
    //TODO useFrameworks with a callback with as argument a fw ref

    class Framework {
    public:
        Framework();
        ~Framework();
        Framework(Framework &&rhs);
        Framework& operator=(Framework&& rhs);

        Framework(const Framework& rhs) = delete;
        Framework& operator=(const Framework &rhs) = delete;


        template<typename T>
        long installBundle(std::string symbolicName, celix::Properties manifest = {}, bool autoStart = true) {
            std::shared_ptr<celix::IBundleActivator> activator{new T{}};
            return installBundle(std::move(symbolicName), std::move(activator), std::move(manifest), autoStart);
        }

        long installBundle(std::string symbolicName, std::shared_ptr<celix::IBundleActivator> activator, celix::Properties manifest = {}, bool autoStart = true);


        //long installBundle(const std::string &path);
        bool startBundle(long bndId);
        bool stopBundle(long bndId);
        bool uninstallBundle(long bndId);

        bool useBundle(long bndId, std::function<void(const celix::IBundle &bnd)> use) const;
        int useBundles(std::function<void(const celix::IBundle &bnd)> use, bool includeFrameworkBundle = false) const;

        //long bundleIdForName(const std::string &bndName) const;
        std::vector<long> listBundles(bool includeFrameworkBundle = false) const;

        celix::ServiceRegistry& registry(const std::string &lang);
    private:
        class Impl;
        std::unique_ptr<Impl> pimpl;
    };

};

#endif //CXX_CELIX_FRAMEWORK_H
