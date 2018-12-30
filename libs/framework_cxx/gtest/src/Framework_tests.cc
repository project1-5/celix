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

#include "celix/Framework.h"

class FrameworkTest : public ::testing::Test {
public:
    FrameworkTest() {}
    ~FrameworkTest(){}

    celix::Framework& framework() { return fw; }
private:
    celix::Framework fw{};
};


TEST_F(FrameworkTest, CreateDestroy) {
    //no bundles installed bundle (framework bundle)
    EXPECT_EQ(1, framework().listBundles(true).size());
    EXPECT_EQ(0, framework().listBundles(false).size());

    bool isFramework = false;
    framework().useBundle(0L, [&](const celix::IBundle &bnd) {
       isFramework = bnd.isFrameworkBundle();
    });
    EXPECT_TRUE(isFramework);
}

TEST_F(FrameworkTest, InstallBundle) {

    class EmbeddedActivator : public celix::IBundleActivator {
    public:
        virtual ~EmbeddedActivator() = default;

        bool resolve(std::shared_ptr<celix::IBundleContext> ctx) noexcept override {
            EXPECT_GE(ctx->bundle()->id(), 1);
            resolveCalled = true;
            return true;
        }

        bool start(std::shared_ptr<celix::IBundleContext>) noexcept override {
            startCalled = true;
            return true;
        }

        bool stop(std::shared_ptr<celix::IBundleContext>) noexcept override {
            stopCalled = true;
            return true;
        }

        bool resolveCalled = false;
        bool startCalled = false;
        bool stopCalled = false;
    };

    long bndId1 = framework().installBundle<EmbeddedActivator>("embedded");
    EXPECT_GE(bndId1, 0);

    std::shared_ptr<EmbeddedActivator> act{new EmbeddedActivator};
    long bndId2 = framework().installBundle("embedded2", act);
    EXPECT_GE(bndId2, 0);
    EXPECT_NE(bndId1, bndId2);
    EXPECT_TRUE(act->resolveCalled);
    EXPECT_TRUE(act->startCalled);
    EXPECT_FALSE(act->stopCalled);

    framework().stopBundle(bndId2);
    EXPECT_TRUE(act->stopCalled);

    std::shared_ptr<EmbeddedActivator> act3{new EmbeddedActivator};
    {
        celix::Framework fw{};
        fw.installBundle("embedded3", act3);
        EXPECT_TRUE(act3->resolveCalled);
        EXPECT_TRUE(act3->startCalled);
        EXPECT_FALSE(act3->stopCalled);

        //NOTE fw out of scope -> bundle stopped
    }
    EXPECT_TRUE(act3->stopCalled);
}

TEST_F(FrameworkTest, StaticBundleTest) {
    class EmbeddedActivator : public celix::IBundleActivator {
    public:
        virtual ~EmbeddedActivator() = default;

        bool start(std::shared_ptr<celix::IBundleContext>) noexcept override {
            return true;
        }
    };

    int count = 0;
    auto factory = [&]() -> celix::IBundleActivator * {
        count++;
        return new EmbeddedActivator{};
    };

    EXPECT_EQ(0, framework().listBundles().size()); //no bundles installed;
    celix::StaticBundleOptions opts;
    opts.bundleActivatorFactory = std::move(factory);
    celix::registerStaticBundle("static", opts);
    EXPECT_EQ(1, framework().listBundles().size()); //static bundle instance installed
    EXPECT_EQ(1, count);

    celix::Framework fw{};
    EXPECT_EQ(1, framework().listBundles().size()); //already registered static bundle instance installed.
    EXPECT_EQ(2, count);
}