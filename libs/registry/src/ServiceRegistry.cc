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
#include <set>
#include <utility>
#include <thread>

#include <glog/logging.h>

#include "celix/Constants.h"
#include "celix/ServiceRegistry.h"
#include "celix/Filter.h"

static std::string emptyString{};

namespace {

    class EmptyBundle : public celix::IResourceBundle {
    public:
        ~EmptyBundle() override = default;

        long id() const noexcept override { return 0; }

        const std::string& root() const noexcept override {
            static std::string empty{};
            return empty;
        }

        bool has(const std::string&) const noexcept override { return false; }
        bool isDir(const std::string&) const noexcept override { return false; }
        bool isFile(const std::string&) const noexcept override { return false; }

        //std::istream& open(const std::string &path) override {}
        //std::fstream& open(const std::string &path) override {}
        std::vector<std::string> readDir(const std::string&) const noexcept override { return std::vector<std::string>{};}
    };

    struct SvcEntry {
        explicit SvcEntry(std::shared_ptr<const celix::IResourceBundle> _owner, long _svcId, std::string _svcName, std::shared_ptr<void> _svc, std::shared_ptr<celix::IServiceFactory<void>> _svcFac,
                 celix::Properties &&_props) :
                owner{std::move(_owner)}, svcId{_svcId}, svcName{std::move(_svcName)},
                props{std::forward<celix::Properties>(_props)},
                ranking{celix::getProperty(props, celix::SERVICE_RANKING, 0L)},
                svc{_svc}, svcFactory{_svcFac} {}

        SvcEntry(SvcEntry &&rhs) = delete;
        SvcEntry &operator=(SvcEntry &&rhs) = delete;
        SvcEntry(const SvcEntry &rhs) = delete;
        SvcEntry &operator=(const SvcEntry &rhs) = delete;

        const std::shared_ptr<const celix::IResourceBundle> owner;
        const long svcId;
        const std::string svcName;
        const celix::Properties props;
        const long ranking;

        void *service(const celix::IResourceBundle &requester) const {
            if (svcFactory) {
                return svcFactory->getService(requester, props);
            } else {
                return svc.get();
            }
        }

        bool factory() const { return svcFactory != nullptr; }

        void incrUsage() const {
            LOG(WARNING) << "TODO use shared_ptr unique instead ?? how to sync?";
            std::lock_guard<std::mutex> lck{mutex};
            usage += 1;
        }

        void decrUsage() const {
            LOG(WARNING) << "TODO use shared_ptr unique instead ?? how is sync?";
            std::lock_guard<std::mutex> lck{mutex};
            usage -= 1;
            cond.notify_all();
        }

        void waitTillUnused() const {
            std::unique_lock<std::mutex> lock{mutex};
            cond.wait(lock, [this]{return usage == 0;});
        }
    private:
        //svc, svcSharedPtr or svcFactory is set
        const std::shared_ptr<void> svc;
        const std::shared_ptr<celix::IServiceFactory<void>> svcFactory;


        //sync TODO refactor to atomics
        mutable std::mutex mutex{};
        mutable std::condition_variable cond{};
        mutable int usage{1};
    };

    struct SvcEntryLess {
        bool operator()( const std::shared_ptr<const SvcEntry>& lhs, const std::shared_ptr<const SvcEntry>& rhs ) const {
            if (lhs->ranking == rhs->ranking) {
                return lhs->svcId < rhs->svcId;
            } else {
                return lhs->ranking > rhs->ranking; //note inverse -> higher rank first in set
            }
        }
    };

    struct SvcTrackerEntry {
        const long id;
        const std::shared_ptr<const celix::IResourceBundle> owner;
        const std::string svcName;
        const celix::ServiceTrackerOptions<void> opts;
        const celix::Filter filter;

        explicit SvcTrackerEntry(long _id, std::shared_ptr<const celix::IResourceBundle> _owner, std::string _svcName, celix::ServiceTrackerOptions<void> _opts) :
            id{_id}, owner{std::move(_owner)}, svcName{std::move(_svcName)}, opts{std::move(_opts)}, filter{opts.filter} {}

        SvcTrackerEntry(SvcTrackerEntry &&rhs) = delete;
        SvcTrackerEntry &operator=(SvcTrackerEntry &&rhs) = delete;
        SvcTrackerEntry(const SvcTrackerEntry &rhs) = delete;
        SvcTrackerEntry &operator=(const SvcTrackerEntry &rhs) = delete;
        ~SvcTrackerEntry() {}

        void clear() {
            //TODO update, make special rem (e.g. only call the use set callbacks once with a nullptr)
            std::vector<std::shared_ptr<const SvcEntry>> removeEntries{};
            {
                std::lock_guard<std::mutex> lck{tracked.mutex};
                for (auto &entry : tracked.entries) {
                    removeEntries.push_back(entry);
                }
                tracked.entries.clear();
            }
            for (auto &entry : removeEntries) {
                remMatch(entry); //note fill try to erase entry from entries again, TODO check if this is safe
            }
        }

        bool valid() const {
            return !svcName.empty() && filter.valid();
        }

        bool match(const SvcEntry& entry) const {
            //note should only called for the correct svcName
            assert(svcName == entry.svcName);

            if (filter.empty()) {
                return true;
            } else {
                return filter.match(entry.props);
            }
        }

        void addMatch(std::shared_ptr<const SvcEntry> entry) {
            //increase usage so that services cannot be removed while a service tracker is still active
            entry->incrUsage();

            {
                std::lock_guard<std::mutex> lck{tracked.mutex};
                tracked.entries.insert(entry);
            }

            //call callbacks
            callSetCallbacks();
            callAddRemoveCallbacks(entry, true);
            callUpdateCallbacks();
        }

        void remMatch(const std::shared_ptr<const SvcEntry> &entry) {
            {
                std::lock_guard<std::mutex> lck{tracked.mutex};
                tracked.entries.erase(entry);
            }

            //call callbacks
            callSetCallbacks();
            callAddRemoveCallbacks(entry, false);
            callUpdateCallbacks();

            //decrease usage so that services cannot be removed while a service tracker is still active
            entry->decrUsage();
        }

        void callAddRemoveCallbacks(const std::shared_ptr<const SvcEntry> &updatedEntry, bool add) {
            auto &update = add ? opts.add : opts.remove;
            if (update != nullptr) {
                void *svc = updatedEntry->service(*owner);
                update(svc);
            }
            //TODO rest of add/remove
        }

        void callSetCallbacks() {
            std::shared_ptr<const SvcEntry> currentHighest;
            bool highestUpdated = false;
            {
                std::lock_guard<std::mutex> lck{tracked.mutex};
                auto begin = tracked.entries.begin();
                if (begin == tracked.entries.end()) {
                    currentHighest = nullptr;
                } else {
                    currentHighest = *begin;
                }
                if (currentHighest != tracked.highest) {
                    tracked.highest = currentHighest;
                    highestUpdated = true;
                }
            }

            //TODO race condition. highest can be updated because lock is released.

            if (highestUpdated) {
                void *svc = currentHighest == nullptr ? nullptr : currentHighest->service(*owner);
                if (opts.set != nullptr) {
                    opts.set(svc);
                }
                //TODO rest of set
            }
        }

        void callUpdateCallbacks() {
            if (opts.update) {
                std::vector<void *> rankedServices{};
                {
                    std::lock_guard<std::mutex> lck{tracked.mutex};
                    rankedServices.reserve(tracked.entries.size());
                    for (auto &tracked : tracked.entries) {
                        rankedServices.push_back(tracked->service(*owner));
                    }
                }
                opts.update(std::move(rankedServices));
            }
            //TODO rest of the update calls
        }

        int count() const {
            LOG(INFO) << "TODO use shared_ptr count instead";
            std::lock_guard<std::mutex> lck{tracked.mutex};
            return (int)tracked.entries.size();
        }

        void incrUsage() const {
            LOG(INFO) << "TODO use shared_ptr count instead";
            std::lock_guard<std::mutex> lck{mutex};
            usage += 1;
        }

        void decrUsage() const {
            LOG(INFO) << "TODO use shared_ptr count instead";
            std::lock_guard<std::mutex> lck{mutex};
            usage -= 1;
            cond.notify_all();
        }

        void waitTillUnused() const {
            std::unique_lock<std::mutex> lock{mutex};
            cond.wait(lock, [this]{return usage == 0;});
        }
    private:
        struct {
            mutable std::mutex mutex; //protects matchedEntries & highestRanking
            std::set<std::shared_ptr<const SvcEntry>, SvcEntryLess> entries{};
            std::shared_ptr<const SvcEntry> highest{};
        } tracked{};


        //sync TODO refactor to atomics
        mutable std::mutex mutex{};
        mutable std::condition_variable cond{};
        mutable int usage{1};
    };
}

/**********************************************************************************************************************
  Impl classes
 **********************************************************************************************************************/

class celix::ServiceRegistration::Impl {
public:
    const std::shared_ptr<const SvcEntry> entry;
    const std::function<void()> unregisterCallback;
    bool registered; //TODO make atomic?
};

class celix::ServiceTracker::Impl {
public:
    const std::shared_ptr<SvcTrackerEntry> entry;
    const std::function<void()> untrackCallback;
    bool active; //TODO make atomic?
};

class celix::ServiceRegistry::Impl {
public:
    const std::shared_ptr<const celix::IResourceBundle> emptyBundle = std::shared_ptr<const celix::IResourceBundle>{new EmptyBundle{}};
    std::string regName;

    struct {
        mutable std::mutex mutex{};
        long nextSvcId = 1L;

        /* Services entries are always stored for it specific svc name.
         * When using services is it always expected that the svc name is provided, and as such
         * storing per svc name is more logical.
         *
         * Note that this also means that the classic 99% use cases used OSGi filter (objectClass=<svcName>)
         * is not needed anymore -> simpler and faster.
         *
         * map key is svcName and the map value is a set of SvcEntry structs.
         * note: The SvcEntry set is ordered (i.e. not a hashset with O(1) access) to ensure that service ranking and
         * service id can be used for their position in the set (see SvcEntryLess).
         */
        std::unordered_map<std::string, std::set<std::shared_ptr<const SvcEntry>, SvcEntryLess>> registry{};

        //Cache map for faster and easier access based on svcId.
        std::unordered_map<long, std::shared_ptr<const SvcEntry>> cache{};
    } services{};

    struct {
        mutable std::mutex mutex{};
        long nextTrackerId = 1L;

        /* Note same ordering as services registry, expect the ranking order requirement,
         * so that it is easier and faster to update the trackers.
         */
        std::unordered_map<std::string, std::set<std::shared_ptr<SvcTrackerEntry>>> registry{};
        std::unordered_map<long, std::shared_ptr<SvcTrackerEntry>> cache{};
    } trackers{};

    celix::ServiceRegistration registerService(std::string serviceName, std::shared_ptr<void> svc, std::shared_ptr<celix::IServiceFactory<void>> factory, celix::Properties props, std::shared_ptr<const celix::IResourceBundle> owner) {
        props[celix::SERVICE_NAME] = std::move(serviceName);
        std::string &svcName = props[celix::SERVICE_NAME];

        std::lock_guard<std::mutex> lock{services.mutex};
        long svcId = services.nextSvcId++;
        props[celix::SERVICE_ID] = std::to_string(svcId);

        //Add to registry
        std::shared_ptr<const celix::IResourceBundle> bnd = owner ? owner : emptyBundle;

        if (factory) {
            VLOG(1) << "Registering service factory '" << svcName << "' from bundle id " << owner->id() << std::endl;
        } else {
            VLOG(1) << "Registering service '" << svcName << "' from bundle id " << owner->id() << std::endl;
        }

        const auto it = services.registry[svcName].emplace(new SvcEntry{std::move(bnd), svcId, svcName, std::move(svc), std::move(factory), std::move(props)});
        assert(it.second); //should always lead to a new entry
        const std::shared_ptr<const SvcEntry> &entry = *it.first;

        //Add to svcId cache
        services.cache[entry->svcId] = entry;

        //update trackers
        std::thread updateThread{[&]{updateTrackers(entry, true);}};
        updateThread.join();
        entry->decrUsage(); //note usage started at 1 during creation


        //create unregister callback
        std::function<void()> unreg = [this, svcId]() -> void {
            this->unregisterService(svcId);
        };
        auto *impl = new celix::ServiceRegistration::Impl{
                .entry = entry,
                .unregisterCallback = std::move(unreg),
                .registered = true
        };
        return celix::ServiceRegistration{impl};
    }

    void unregisterService(long svcId) {
        if (svcId <= 0) {
            return; //silent ignore
        }

        std::shared_ptr<const SvcEntry> match{nullptr};

        {
            std::lock_guard<std::mutex> lock{services.mutex};
            const auto it = services.cache.find(svcId);
            if (it != services.cache.end()) {
                match = it->second;
                services.cache.erase(it);
                services.registry.at(match->svcName).erase(match);
            }
        }

        if (match) {
            std::thread updateThread{[&]{updateTrackers(match, false);}};
            updateThread.join();
            match->waitTillUnused();
        } else {
            LOG(WARNING) << "Cannot unregister service. Unknown service id: " << svcId << "." << std::endl;
        }
    }

    void removeTracker(long trkId) {
        if (trkId <= 0) {
            return; //silent ignore
        }

        std::shared_ptr<SvcTrackerEntry> match{nullptr};
        {
            std::lock_guard<std::mutex> lock{trackers.mutex};
            const auto it = trackers.cache.find(trkId);
            if (it != trackers.cache.end()) {
                match = it->second;
                trackers.cache.erase(it);
                trackers.registry.at(match->svcName).erase(match);
            }
        }

        if (match) {
            match->waitTillUnused();
            std::thread clearThread{[&]{match->clear();}}; //ensure that all service are removed using the callbacks
            clearThread.join();
        } else {
            LOG(WARNING) << "Cannot remove tracker. Unknown tracker id: " << trkId << "." << std::endl;
        }
    }

    void updateTrackers(const std::shared_ptr<const SvcEntry> &entry, bool adding) {
        std::vector<std::shared_ptr<SvcTrackerEntry>> matchedTrackers{};
        {
            std::lock_guard<std::mutex> lock{trackers.mutex};
            for (auto &tracker : trackers.registry[entry->svcName]) {
                if (tracker->match(*entry)) {
                    tracker->incrUsage();
                    matchedTrackers.push_back(tracker);
                }
            }
        }

        for (auto &match : matchedTrackers) {
            if (adding) {
                match->addMatch(entry);
            } else {
                match->remMatch(entry);
            }
            match->decrUsage();
        }
    }
};



/**********************************************************************************************************************
  Service Registry
 **********************************************************************************************************************/


celix::ServiceRegistry::ServiceRegistry(std::string name) : pimpl{new ServiceRegistry::Impl{}} {
    pimpl->regName = std::move(name);
}
celix::ServiceRegistry::ServiceRegistry(celix::ServiceRegistry &&rhs) = default;
celix::ServiceRegistry& celix::ServiceRegistry::operator=(celix::ServiceRegistry &&rhs) = default;
celix::ServiceRegistry::~ServiceRegistry() {
    if (pimpl) {
        //TODO
    }
}

const std::string& celix::ServiceRegistry::name() const { return pimpl->regName; }

/*
celix::ServiceRegistration celix::ServiceRegistry::registerService(const std::string &svcName, std::unique_ptr<void> svc, celix::Properties props, std::shared_ptr<const celix::IResourceBundle> owner) {
    return pimpl->registerService(std::move(svcName), nullptr, {}, std::move(svc), std::move(props), std::move(owner));
}*/

celix::ServiceRegistration celix::ServiceRegistry::registerService(std::string svcName, std::shared_ptr<void> svc, celix::Properties props, std::shared_ptr<const celix::IResourceBundle> owner) {
    return pimpl->registerService(std::move(svcName), std::move(svc), {}, std::move(props), std::move(owner));
}

celix::ServiceRegistration celix::ServiceRegistry::registerServiceFactory(std::string svcName, std::shared_ptr<celix::IServiceFactory<void>> factory, celix::Properties props, std::shared_ptr<const celix::IResourceBundle> owner) {
    return pimpl->registerService(std::move(svcName), {}, std::move(factory), std::move(props), std::move(owner));
}

//TODO add useService(s) call to ServiceTracker object for fast service access

//TODO move to Impl
celix::ServiceTracker celix::ServiceRegistry::trackServices(std::string svcName, celix::ServiceTrackerOptions<void> options, std::shared_ptr<const celix::IResourceBundle> requester) {
    //TODO create new tracker event and start new thread to update track trackers
    long trkId = 0;
    {
        std::lock_guard<std::mutex> lck{pimpl->trackers.mutex};
        trkId = pimpl->trackers.nextTrackerId++;
    }

    auto trkEntry = std::shared_ptr<SvcTrackerEntry>{new SvcTrackerEntry{trkId, requester, std::move(svcName), std::move(options)}};
    if (trkEntry->valid()) {

        //find initial services and add new tracker to registry.
        //NOTE two locks to ensure no new services can be added/removed during initial tracker setup
        std::vector<std::shared_ptr<const SvcEntry>> services{};
        {
            std::lock_guard<std::mutex> lck1{pimpl->services.mutex};
            for (auto &svcEntry : pimpl->services.registry[trkEntry->svcName]) {
                if (trkEntry->match(*svcEntry)) {
                    svcEntry->incrUsage();
                    services.push_back(svcEntry);
                }
            }

            std::lock_guard<std::mutex> lck2{pimpl->trackers.mutex};
            pimpl->trackers.registry[trkEntry->svcName].insert(trkEntry);
            pimpl->trackers.cache[trkEntry->id] = trkEntry;
        }
        std::thread updateThread{[&]{
            for (auto &svcEntry : services) {
                trkEntry->addMatch(svcEntry);
                svcEntry->decrUsage();
            }
        }};
        updateThread.join();
        trkEntry->decrUsage(); //note trkEntry usage started at 1

        auto untrack = [this, trkId]() -> void {
            this->pimpl->removeTracker(trkId);
        };
        auto *impl = new celix::ServiceTracker::Impl{
                .entry = trkEntry,
                .untrackCallback = std::move(untrack),
                .active = true
        };
        return celix::ServiceTracker{impl};
    } else {
        trkEntry->decrUsage(); //note usage is 1 at creation
        LOG(ERROR) << "Cannot create tracker. Invalid filter?" << std::endl;
        return celix::ServiceTracker{nullptr};
    }
}

//TODO move to Impl
long celix::ServiceRegistry::nrOfServiceTrackers() const {
    std::lock_guard<std::mutex> lck{pimpl->trackers.mutex};
    return pimpl->trackers.cache.size();
}
        
//TODO unregister tracker with remove tracker event in a new thread
//TODO move to Impl
std::vector<long> celix::ServiceRegistry::findServices(const std::string &svcName, const std::string &rawFilter) const {
    std::vector<long> result{};
    celix::Filter filter = rawFilter;
    if (!filter.valid()) {
        LOG(WARNING) << "Invalid filter (" << rawFilter << ") provided. Cannot find services" << std::endl;
        return result;
    }

    std::lock_guard<std::mutex> lock{pimpl->services.mutex};
    const auto it = pimpl->services.registry.find(svcName);
    if (it != pimpl->services.registry.end()) {
        const auto &services = it->second;
        for (const auto &visit : services) {
            if (filter.empty() || filter.match(visit->props)) {
                result.push_back(visit->svcId);
            }
        }
    }
    return result;
}

//TODO move to Impl
long celix::ServiceRegistry::nrOfRegisteredServices() const {
    std::lock_guard<std::mutex> lock{pimpl->services.mutex};
    return pimpl->services.cache.size();
}

//TODO move to Impl
int celix::ServiceRegistry::useServices(const std::string &svcName, std::function<void(void *svc, const celix::Properties &props, const celix::IResourceBundle &bnd)> &use, const std::string &rawFilter, std::shared_ptr<const celix::IResourceBundle> requester) const {
    celix::Filter filter = rawFilter;
    if (!filter.valid()) {
        LOG(WARNING) << "Invalid filter (" << rawFilter << ") provided. Cannot find services" << std::endl;
        return 0;
    }

    std::vector<std::shared_ptr<const SvcEntry>> matches{};
    {
        std::lock_guard<std::mutex> lock{pimpl->services.mutex};
        const auto it = pimpl->services.registry.find(svcName);
        if (it != pimpl->services.registry.end()) {
            const auto &services = it->second;
            for (const std::shared_ptr<const SvcEntry> &entry : services) {
                if (filter.empty() || filter.match(entry->props)) {
                    entry->incrUsage();
                    matches.push_back(entry);
                }
            }
        }
    }

    for (const std::shared_ptr<const SvcEntry> &entry : matches) {
        use(entry->service(*requester), entry->props, *entry->owner);
        entry->decrUsage();
    }

    return (int)matches.size();
}

//TODO move to Impl
bool celix::ServiceRegistry::useService(const std::string &svcName, std::function<void(void *svc, const celix::Properties &props, const celix::IResourceBundle &bnd)> &use, const std::string &rawFilter, std::shared_ptr<const celix::IResourceBundle> requester) const {
    celix::Filter filter = rawFilter;
    if (!filter.valid()) {
        LOG(WARNING) << "Invalid filter (" << rawFilter << ") provided. Cannot find services" << std::endl;
        return false;
    }

    std::shared_ptr<const SvcEntry> match = nullptr;
    {
        std::lock_guard<std::mutex> lock{pimpl->services.mutex};
        const auto it = pimpl->services.registry.find(svcName);
        if (it != pimpl->services.registry.end()) {
            const auto &services = it->second;
            for (const std::shared_ptr<const SvcEntry> &visit : services) {
                if (filter.empty() || filter.match(visit->props)) {
                    visit->incrUsage();
                    match = visit;
                    break;
                }
            }
        }
    }

    if (match != nullptr) {
        use(match->service(*requester), match->props, *match->owner);
        match->decrUsage();
    }

    return match != nullptr;
}

/**********************************************************************************************************************
  Service Registration
 **********************************************************************************************************************/

celix::ServiceRegistration::ServiceRegistration() : pimpl{nullptr} {}

celix::ServiceRegistration::ServiceRegistration(celix::ServiceRegistration::Impl *impl) : pimpl{impl} {}
celix::ServiceRegistration::ServiceRegistration(celix::ServiceRegistration &&rhs) noexcept = default;
celix::ServiceRegistration& celix::ServiceRegistration::operator=(celix::ServiceRegistration &&rhs) noexcept = default;
celix::ServiceRegistration::~ServiceRegistration() { unregister(); }

long celix::ServiceRegistration::serviceId() const { return pimpl ? pimpl->entry->svcId : -1L; }
bool celix::ServiceRegistration::valid() const { return serviceId() >= 0; }
bool celix::ServiceRegistration::factory() const { return pimpl && pimpl->entry->factory(); }
bool celix::ServiceRegistration::registered() const {return pimpl && pimpl->registered; }

void celix::ServiceRegistration::unregister() {
    if (pimpl && pimpl->registered) {
        pimpl->registered = false; //TODO make thread safe
        pimpl->unregisterCallback();
    }
}

const celix::Properties& celix::ServiceRegistration::properties() const {
    static const celix::Properties empty{};
    return pimpl ? pimpl->entry->props : empty;
}

const std::string& celix::ServiceRegistration::serviceName() const {
    static const std::string empty{};
    if (pimpl) {
        return celix::getProperty(pimpl->entry->props, celix::SERVICE_NAME, empty);
    }
    return empty;
}




/**********************************************************************************************************************
  Service Tracker
 **********************************************************************************************************************/

celix::ServiceTracker::ServiceTracker() : pimpl{nullptr} {}

celix::ServiceTracker::ServiceTracker(celix::ServiceTracker::Impl *impl) : pimpl{impl} {}

celix::ServiceTracker::~ServiceTracker() {
    if (pimpl && pimpl->active) {
        pimpl->untrackCallback();
    }
}


void celix::ServiceTracker::stop() {
    if (pimpl && pimpl->active) { //TODO make thread safe
        pimpl->untrackCallback();
        pimpl->active = false;
    }
}

celix::ServiceTracker::ServiceTracker(celix::ServiceTracker &&rhs) noexcept = default;
celix::ServiceTracker& celix::ServiceTracker::operator=(celix::ServiceTracker &&rhs) noexcept = default;

int celix::ServiceTracker::trackCount() const { return pimpl ? pimpl->entry->count() : 0; }
const std::string& celix::ServiceTracker::serviceName() const { return pimpl? pimpl->entry->svcName : emptyString; }
const std::string& celix::ServiceTracker::filter() const { return pimpl ? pimpl->entry->filter.filterStr : emptyString; }
bool celix::ServiceTracker::valid() const { return pimpl != nullptr; }