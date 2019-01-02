#include <utility>

#include <utility>

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

#include <unordered_map>
#include <mutex>
#include <iostream>
#include <set>
#include <vector>
#include <future>

#include <glog/logging.h>

#include "celix/Framework.h"

#include "BundleController.h"

struct StaticBundleEntry {
    const std::string symbolicName;
    const celix::Properties manifest;
    const std::function<celix::IBundleActivator*(std::shared_ptr<celix::BundleContext>)> activatorFactory;
};

static struct {
    std::mutex mutex{};
    std::vector<StaticBundleEntry> bundles{};
    std::set<celix::Framework *> frameworks{};
} staticRegistry{};


static void registerFramework(celix::Framework *fw);
static void unregisterFramework(celix::Framework *fw);

class celix::Framework::Impl : public IBundle {
public:
    Impl(celix::Framework *_fw, celix::Properties _config) : fw{_fw}, config{std::move(_config)}, bndManifest{createManifest()}, cwd{createCwd()} {}

    ~Impl() {
        stopFramework();
        waitForShutdown();
    }

    std::vector<long> listBundles(bool includeFrameworkBundle) const {
        std::vector<long> result{};
        if (includeFrameworkBundle) {
            result.push_back(0L); //framework bundle id
        }
        std::lock_guard<std::mutex> lock{bundles.mutex};
        for (auto &entry : bundles.entries) {
            result.push_back(entry.first);
        }
        std::sort(result.begin(), result.end());//ensure that the bundles are order by bndId -> i.e. time of install
        return result;
    }

    long installBundle(std::string symbolicName, std::function<celix::IBundleActivator*(std::shared_ptr<celix::BundleContext>)> actFactory, celix::Properties manifest, bool autoStart) {
        //TODO if activator is nullptr -> use empty activator
        //TODO on separate thread ?? specific bundle resolve thread ??
        long bndId = -1L;
        if (symbolicName.empty()) {
            LOG(WARNING) << "Cannot install bundle with a empty symbolic name" << std::endl;
            return bndId;
        }

        std::shared_ptr<celix::BundleController> bndController{nullptr};
        {
            manifest[celix::MANIFEST_BUNDLE_SYMBOLIC_NAME] = symbolicName;
            if (manifest.find(celix::MANIFEST_BUNDLE_NAME) == manifest.end()) {
                manifest[celix::MANIFEST_BUNDLE_NAME] = symbolicName;
            }
            if (manifest.find(celix::MANIFEST_BUNDLE_VERSION) == manifest.end()) {
                manifest[celix::MANIFEST_BUNDLE_NAME] = "0.0.0";
            }
            if (manifest.find(celix::MANIFEST_BUNDLE_GROUP) == manifest.end()) {
                manifest[celix::MANIFEST_BUNDLE_GROUP] = "";
            }

            std::lock_guard<std::mutex> lck{bundles.mutex};
            bndId = bundles.nextBundleId++;
            auto bnd = std::shared_ptr<celix::Bundle>{new celix::Bundle{bndId, this->fw, std::move(manifest)}};
            auto ctx = std::shared_ptr<celix::BundleContext>{new celix::BundleContext{bnd}};
            bndController = std::shared_ptr<celix::BundleController>{new celix::BundleController{std::move(actFactory), bnd, ctx}};
            bundles.entries.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(bndId),
                                     std::forward_as_tuple(bndController));

            //TODO increase bnd entry usage
        }

        if (bndController) {
            if (autoStart) {
                bool successful = bndController->transitionTo(BundleState::ACTIVE);
                if (!successful) {
                    LOG(WARNING) << "Cannot start bundle " << bndController->bundle()->symbolicName() << std::endl;
                }
            }
        }

        return bndId;
    }

    bool startBundle(long bndId) {
        if (bndId == this->fwBndId) {
            //TODO
            return false;
        } else {
            return transitionBundleTo(bndId, BundleState::ACTIVE);
        }
    }

    bool stopBundle(long bndId) {
        if (bndId == this->fwBndId) {
            return stopFramework();
        } else {
            return transitionBundleTo(bndId, BundleState::INSTALLED);
        }
    }

    bool uninstallBundle(long bndId) {
        bool uninstalled = false;
        std::shared_ptr<celix::BundleController> removed{nullptr};
        {
            std::lock_guard<std::mutex> lck{bundles.mutex};
            auto it = bundles.entries.find(bndId);
            if (it != bundles.entries.end()) {
                removed = std::move(it->second);
                bundles.entries.erase(it);

            }
        }
        if (removed) {
            bool stopped = removed->transitionTo(BundleState::INSTALLED);
            if (stopped) {
                //TODO check and wait till bundle is not used anymore. is this needed (shared_ptr) or just let access
                //to filesystem fail ...
            } else {
                //add bundle again -> not uninstalled
                std::lock_guard<std::mutex> lck{bundles.mutex};
                bundles.entries[bndId] = std::move(removed);
            }
        }
        return uninstalled;
    }

    bool transitionBundleTo(long bndId, BundleState desired) {
        bool successful = false;
        std::shared_ptr<celix::BundleController> match{nullptr};
        {
            std::lock_guard<std::mutex> lck{bundles.mutex};
            auto it = bundles.entries.find(bndId);
            if (it != bundles.entries.end()) {
                match = it->second;
            }
        }
        if (match) {
            successful = match->transitionTo(desired);
        }
        return successful;
    }

    bool useBundle(long bndId, std::function<void(const celix::IBundle &bnd)> use) const {
        bool called = false;
        if (bndId == 0) {
            //framework bundle
            use(*this);
            called = true;
        } else {
            std::shared_ptr<celix::BundleController> match = nullptr;
            {
                std::lock_guard<std::mutex> lck{bundles.mutex};
                auto it = bundles.entries.find(bndId);
                if (it != bundles.entries.end()) {
                    match = it->second;
                    //TODO increase usage
                }
            }
            if (match) {
                use(*match->bundle());
                called = true;
                //TODO decrease usage -> use shared ptr instead
            }
        }
        return called;
    }

    int useBundles(std::function<void(const celix::IBundle &bnd)> use, bool includeFramework) const {
        std::map<long, std::shared_ptr<celix::BundleController>> useBundles{};
        {
            std::lock_guard<std::mutex> lck{bundles.mutex};
            for (const auto &it : bundles.entries) {
                useBundles[it.first] = it.second;
            }
        }

        if (includeFramework) {
            use(*this);
        }
        for (const auto &cntr : useBundles) {
            use(*cntr.second->bundle());
        }
        int count = (int)useBundles.size();
        if (includeFramework) {
            count += 1;
        }
        return count;
    }

    celix::ServiceRegistry& registry(const std::string &lang) {
        std::lock_guard<std::mutex> lck{registries.mutex};
        auto it = registries.entries.find(lang);
        if (it == registries.entries.end()) {
            registries.entries.emplace(std::string{lang}, celix::ServiceRegistry{std::string{lang}});
            return registries.entries.at(lang);
        } else {
            return it->second;
        }
    }

    //resource bundle part
    long id() const noexcept override { return 1L /*note registry empty bundle is id 0, framework is id 1*/; }
    bool has(const std::string&) const override { return false; }
    bool isDir(const std::string&) const override { return false; }
    bool isFile(const std::string&) const override { return false; }
    std::vector<std::string> readDir(const std::string&) const override { return std::vector<std::string>{};}
    const std::string& root() const noexcept override { //TODO
        return cwd;
    }

    //bundle stuff
    bool isFrameworkBundle() const noexcept override { return true; }
    void* handle() const noexcept override { return nullptr; }
    celix::BundleState state() const noexcept override { return BundleState::ACTIVE ; }
    const std::string& name() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_NAME); }
    const std::string& symbolicName() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_SYMBOLIC_NAME); }
    const std::string& group() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_GROUP); }
    const std::string& version() const noexcept override { return bndManifest.at(celix::MANIFEST_BUNDLE_VERSION); }
    const celix::Properties& manifest() const noexcept  override { return bndManifest;}
    bool isValid() const noexcept override { return true; }
    celix::Framework& framework() const noexcept override { return *fw; }

    bool stopFramework() {
        std::lock_guard<std::mutex> lck{shutdown.mutex};
        if (!shutdown.shutdownStarted) {
            shutdown.future = std::async(std::launch::async, [this]{
                std::vector<long> bundles = listBundles(false);
                while (!bundles.empty()) {
                    for (auto it = bundles.rbegin(); it != bundles.rend(); ++it) {
                        stopBundle(*it);
                        uninstallBundle(*it);
                    }
                    bundles = listBundles(false);
                }
            });
            shutdown.shutdownStarted = true;
            shutdown.cv.notify_all();
        }
        return true;
    }

    bool waitForShutdown() const {
        std::unique_lock<std::mutex> lck{shutdown.mutex};
        shutdown.cv.wait(lck, [this]{return this->shutdown.shutdownStarted;});
        shutdown.future.wait();
        lck.unlock();
        return true;
    }
private:
    celix::Properties createManifest() {
        celix::Properties m{};
        m[celix::MANIFEST_BUNDLE_SYMBOLIC_NAME] = "framework";
        m[celix::MANIFEST_BUNDLE_NAME] = "Framework";
        m[MANIFEST_BUNDLE_GROUP] = "Celix";
        m[celix::MANIFEST_BUNDLE_VERSION] = "3.0.0";
        return m;
    }

    std::string createCwd() {
        char workdir[PATH_MAX];
        if (getcwd(workdir, sizeof(workdir)) != NULL) {
            return std::string{workdir};
        } else {
            return std::string{};
        }
    }

    const long fwBndId = 1L;
    celix::Framework * const fw;
    const celix::Properties config;
    const celix::Properties bndManifest;
    const std::string cwd;


    struct {
        mutable std::mutex mutex{};
        mutable std::condition_variable cv{};
        std::future<void> future{};
        bool shutdownStarted = false;
    } shutdown{};



    struct {
        std::unordered_map<long, std::shared_ptr<celix::BundleController>> entries{};
        long nextBundleId = 2;
        mutable std::mutex mutex{};
    } bundles{};

    struct {
        mutable std::mutex mutex{};
        std::unordered_map<std::string, celix::ServiceRegistry> entries;
    } registries;
};

/***********************************************************************************************************************
 * Framework
 **********************************************************************************************************************/

celix::Framework::Framework(celix::Properties config) {
    pimpl = std::unique_ptr<Impl>{new Impl{this, std::move(config)}};
    registerFramework(this);
}
celix::Framework::~Framework() {
    unregisterFramework(this);
}
celix::Framework::Framework(Framework &&rhs) = default;
celix::Framework& celix::Framework::operator=(Framework&& rhs) = default;


long celix::Framework::installBundle(std::string name, std::function<celix::IBundleActivator*(std::shared_ptr<celix::BundleContext>)> actFactory, celix::Properties manifest, bool autoStart) {
    return pimpl->installBundle(std::move(name), actFactory, std::move(manifest), autoStart);
}


std::vector<long> celix::Framework::listBundles(bool includeFrameworkBundle) const { return pimpl->listBundles(includeFrameworkBundle); }

bool celix::Framework::useBundle(long bndId, std::function<void(const celix::IBundle &bnd)> use) const {
    return pimpl->useBundle(bndId, use);
}

int celix::Framework::useBundles(std::function<void(const celix::IBundle &bnd)> use, bool includeFrameworkBundle) const {
    return pimpl->useBundles(use, includeFrameworkBundle);
}

bool celix::Framework::startBundle(long bndId) { return pimpl->stopBundle(bndId); }
bool celix::Framework::stopBundle(long bndId) { return pimpl->stopBundle(bndId); }
bool celix::Framework::uninstallBundle(long bndId) { return pimpl->uninstallBundle(bndId); }
celix::ServiceRegistry& celix::Framework::registry(const std::string &lang) { return pimpl->registry(lang); }

bool celix::Framework::waitForShutdown() const { return pimpl->waitForShutdown(); }

/***********************************************************************************************************************
 * Celix 'global' functions
 **********************************************************************************************************************/

void celix::registerStaticBundle(
        std::string symbolicName,
        std::function<celix::IBundleActivator*(std::shared_ptr<celix::BundleContext>)> bundleActivatorFactory,
        celix::Properties manifest) {
    std::lock_guard<std::mutex> lck{staticRegistry.mutex};
    for (auto fw : staticRegistry.frameworks) {
        fw->installBundle(symbolicName, bundleActivatorFactory, manifest);
    }
    staticRegistry.bundles.emplace_back(StaticBundleEntry{.symbolicName = std::move(symbolicName), .manifest = std::move(manifest), .activatorFactory = std::move(bundleActivatorFactory)});
}

static void registerFramework(celix::Framework *fw) {
    std::lock_guard<std::mutex> lck{staticRegistry.mutex};
    staticRegistry.frameworks.insert(fw);
    for (auto &entry : staticRegistry.bundles) {
        fw->installBundle(entry.symbolicName, entry.activatorFactory, entry.manifest);
    }
}

static void unregisterFramework(celix::Framework *fw) {
    std::lock_guard<std::mutex> lck{staticRegistry.mutex};
    staticRegistry.frameworks.erase(fw);
}