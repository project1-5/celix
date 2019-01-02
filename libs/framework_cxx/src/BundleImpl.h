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

#ifndef CXX_CELIX_IMPL_BUNDLE_H
#define CXX_CELIX_IMPL_BUNDLE_H

#include <glog/logging.h>

#include "celix/IBundle.h"
#include "celix/IBundleContext.h"

namespace celix {
namespace impl {

        class BundleContext : public celix::IBundleContext {
        public:
            BundleContext(std::shared_ptr<celix::IBundle> _bnd) : bnd{std::move(_bnd)},
                reg{&bnd->framework().registry(celix::CXX_LANG)},
                cReg(&bnd->framework().registry(celix::C_LANG)){}

            virtual ~BundleContext() = default;

            std::shared_ptr<celix::IBundle> bundle() const noexcept override {
                return bnd;
            }

            bool useBundle(long bndId, std::function<void(const celix::IBundle &bnd)> use) const noexcept override {
                return bnd->framework().useBundle(bndId, std::move(use));
            }

            int useBundles(std::function<void(const celix::IBundle &bnd)> use, bool includeFrameworkBundle = true) const noexcept override {
                return bnd->framework().useBundles(std::move(use), includeFrameworkBundle);
            }

            bool stopBundle(long bndId) noexcept override {
                return bnd->framework().stopBundle(bndId);
            }

            bool startBundle(long bndId) noexcept override {
                return bnd->framework().startBundle(bndId);
            }

        private:
            celix::ServiceRegistry& registry() const noexcept override { return *reg; }
            celix::ServiceRegistry& cRegistry() const noexcept override { return *cReg; }

            const std::shared_ptr<celix::IBundle> bnd;
            celix::ServiceRegistry * const reg; //TODO make weak_ptr
            celix::ServiceRegistry * const cReg; //TODO make weak_ptr
        };


        class Bundle : public celix::IBundle {
        public:
            Bundle(long _bndId, celix::Framework *_fw, celix::Properties _manifest) :
            bndId{_bndId}, fw{_fw}, bndManifest{std::move(_manifest)} {
                bndState.store(BundleState::INSTALLED, std::memory_order_release);
            }

            //resource part
            bool has(const std::string&) const noexcept override { return false; } //TODO
            bool isDir(const std::string&) const noexcept override { return false; } //TODO
            bool isFile(const std::string&) const noexcept override { return false; } //TODO
            std::vector<std::string> readDir(const std::string&) const noexcept override { return std::vector<std::string>{};} //TODO
            const std::string& root() const noexcept override { //TODO
                static std::string empty{};
                return empty;
            }

            //bundle part
            bool isFrameworkBundle() const noexcept override { return false; }

            void* handle() const noexcept override { return nullptr; } //TODO

            long id() const noexcept override { return bndId; }
            const std::string& name() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_NAME); }
            const std::string& symbolicName() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_SYMBOLIC_NAME); }
            const std::string& group() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_SYMBOLIC_NAME); }
            const std::string& version() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_VERSION); }
            const celix::Properties& manifest() const noexcept  override { return bndManifest;}
            bool isValid() const noexcept override { return bndId >= 0; }
            celix::Framework& framework() const noexcept override { return *fw; }

            celix::BundleState state() const noexcept override {
                return bndState.load(std::memory_order_acquire);
            }

            void setState(celix::BundleState state) {
                bndState.store(state, std::memory_order_release);
            }

        private:
            const long bndId;
            celix::Framework * const fw;
            const celix::Properties bndManifest;
            std::weak_ptr<celix::IBundleContext> context;

            std::atomic<BundleState> bndState;
    };

    class BundleController {
    public:
        BundleController(
                std::function<celix::IBundleActivator*(std::shared_ptr<celix::IBundleContext>)> _actFactory,
                std::shared_ptr<celix::impl::Bundle> _bnd,
                std::shared_ptr<celix::impl::BundleContext> _ctx) :
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

        std::shared_ptr<celix::impl::Bundle> bundle() const { return bnd; }
        std::shared_ptr<celix::impl::BundleContext> context() const { return ctx; }
    private:
        const std::function<celix::IBundleActivator*(std::shared_ptr<celix::IBundleContext>)> actFactory;
        const std::shared_ptr<celix::impl::Bundle> bnd;
        const std::shared_ptr<celix::impl::BundleContext> ctx;

        mutable std::mutex mutex{};
        std::unique_ptr<celix::IBundleActivator> act{nullptr};
    };
}
};

#endif //CXX_CELIX_IMPL_BUNDLE_H
