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

#ifndef CXX_CELIX_BUNDLECONTROLLER_H
#define CXX_CELIX_BUNDLECONTROLLER_H

#include <glog/logging.h>

#include "celix/IBundle.h"
#include "celix/BundleContext.h"
#include "Bundle.h"

namespace celix {
    class BundleController {
    public:
        BundleController(
                std::function<celix::IBundleActivator*(std::shared_ptr<celix::BundleContext>)> _actFactory,
                std::shared_ptr<celix::Bundle> _bnd,
                std::shared_ptr<celix::BundleContext> _ctx) :
                actFactory{std::move(_actFactory)}, bnd{std::move(_bnd)}, ctx{std::move(_ctx)} {}

        //specific part
        bool transitionTo(BundleState desired) {
            bool success = false;
            std::lock_guard<std::mutex> lck{mutex};
            const BundleState state = bnd->state();
            if (state == desired) {
                //nop
                success = true;
            } else if (state == BundleState::INSTALLED && desired == BundleState::ACTIVE) {
                act = std::unique_ptr<celix::IBundleActivator>{actFactory(ctx)};
                bnd->setState(BundleState::ACTIVE);
                success = true;
            } else if (state == BundleState::ACTIVE && desired == BundleState::INSTALLED ) {
                act = nullptr;
                bnd->setState(BundleState::INSTALLED);
                success = true;
            } else {
                //LOG(ERROR) << "Unexpected desired state " << desired << " from state " << bndState << std::endl;
                LOG(ERROR) << "Unexpected desired/form state combination " << std::endl;
            }
            return success;
        }

        std::shared_ptr<celix::Bundle> bundle() const { return bnd; }
        std::shared_ptr<celix::BundleContext> context() const { return ctx; }
    private:
        const std::function<celix::IBundleActivator*(std::shared_ptr<celix::BundleContext>)> actFactory;
        const std::shared_ptr<celix::Bundle> bnd;
        const std::shared_ptr<celix::BundleContext> ctx;

        mutable std::mutex mutex{};
        std::unique_ptr<celix::IBundleActivator> act{nullptr};
    };
}

#endif //CXX_CELIX_BUNDLECONTROLLER_H
