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

#include "gtest/gtest.h"

#include "celix/ServiceRegistry.h"
#include "celix/Constants.h"

class RegistryConcurrentcyTest : public ::testing::Test {
public:
    RegistryConcurrentcyTest() {}
    ~RegistryConcurrentcyTest(){}

    celix::ServiceRegistry& registry() { return reg; }
private:
    celix::ServiceRegistry reg{"C++"};

class ICalc {
public:
    virtual ~ICalc() = default;
    virtual double calc();
};

class NodeCalc : public ICalc {
public:
    virtual ~NodeCalc() = default;

    double calc() override {
        double val = 1.0;
        std::lock_guard<std::mutex> lck{mutex};
        for (auto *calc : childern) {
            val *= calc->calc();
        }
        return val;
    }
private:
    std::mutex mutex{}
    std::vector<ICalc*> childern{};
};

class LeafCalc : public ICalc {
public:
    virtual ~LeafCalc() = default;

    double calc() override {
        return seed;
    }
private:
    double const seed = std::rand() / 100.0;
};


TEST_F(RegistryConcurrentcyTest, ServiceRegistrationTest) {
//TODO start many thread and cals using the tiers of other calcs eventually leading ot leafs and try to break the registry.
//tier 1 : NodeCalc
//tier 2 : NodeCalc
//tier 3 : LeafCalc
}
