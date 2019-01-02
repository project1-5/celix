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

    //TODO resources. resolved from bundle specific symbols which is linked zip file to the library
    void registerStaticBundle(
            std::string symbolicName,
            std::function<celix::IBundleActivator*(std::shared_ptr<celix::IBundleContext>)> bundleActivatorFactory = {},
            celix::Properties manifest = {});

    template<typename T>
    void registerStaticBundle(
            std::string symbolicName,
            celix::Properties manifest = {}) {
        auto actFactory = [](std::shared_ptr<celix::IBundleContext> ctx) {
            return new T{std::move(ctx)};
        };
        celix::registerStaticBundle(std::move(symbolicName), actFactory, std::move(manifest));
    }

    class Framework {
    public:
        Framework(celix::Properties config = {});
        ~Framework();
        Framework(Framework &&rhs);
        Framework& operator=(Framework&& rhs);

        Framework(const Framework& rhs) = delete;
        Framework& operator=(const Framework &rhs) = delete;

        template<typename T>
        long installBundle(std::string name, celix::Properties manifest = {}, bool autoStart = true) {
            auto actFactory = [](std::shared_ptr<celix::IBundleContext> ctx) {
                return new T{std::move(ctx)};
            };
            return installBundle(name, std::move(actFactory), manifest, autoStart);
        }

        long installBundle(std::string name, std::function<celix::IBundleActivator*(std::shared_ptr<celix::IBundleContext>)> actFactory, celix::Properties manifest = {}, bool autoStart = true);


        //long installBundle(const std::string &path);
        bool startBundle(long bndId);
        bool stopBundle(long bndId);
        bool uninstallBundle(long bndId);

        bool useBundle(long bndId, std::function<void(const celix::IBundle &bnd)> use) const;
        int useBundles(std::function<void(const celix::IBundle &bnd)> use, bool includeFrameworkBundle = false) const;

        //long bundleIdForName(const std::string &bndName) const;
        std::vector<long> listBundles(bool includeFrameworkBundle = false) const;

        celix::ServiceRegistry& registry(const std::string &lang);

        bool waitForShutdown() const;
    private:
        class Impl;
        std::unique_ptr<Impl> pimpl;
    };

};

#endif //CXX_CELIX_FRAMEWORK_H
