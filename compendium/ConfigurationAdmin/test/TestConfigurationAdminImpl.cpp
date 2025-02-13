/*=============================================================================

 Library: CppMicroServices

 Copyright (c) The CppMicroServices developers. See the COPYRIGHT
 file at the top-level directory of this distribution and at
 https://github.com/CppMicroServices/CppMicroServices/COPYRIGHT .

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 =============================================================================*/

#include <sstream>

#include "cppmicroservices/BundleContext.h"
#include "cppmicroservices/Framework.h"
#include "cppmicroservices/FrameworkEvent.h"
#include "cppmicroservices/FrameworkFactory.h"
#include "cppmicroservices/cm/ConfigurationException.hpp"

#include "../src/CMAsyncWorkService.hpp"

#include "../src/ConfigurationAdminImpl.hpp"
#include "Mocks.hpp"

namespace cppmicroservices
{
    namespace cmimpl
    {
        MATCHER_P(AnyMapEquals, value, "")
        {
            std::ostringstream argString;
            std::ostringstream valueString;
            cppmicroservices::any_value_to_string(argString, arg);
            cppmicroservices::any_value_to_string(valueString, value);
            return (argString.str() == valueString.str());
        }
        /* This is a ConfigurationListener that blocks on a mutex. It is used by the TestConfigurationAdminImpl
         * VerifyNoUpdateDeadlock test to recreate the blocking condition that caused a deadlock for the
         * Update, UpdateIfDifferent and Remove Configuration methods
         */
        struct BlockingConfigurationListener final : public cppmicroservices::service::cm::ConfigurationListener
        {

            BlockingConfigurationListener() = default;
            BlockingConfigurationListener(BlockingConfigurationListener const&) = delete;
            BlockingConfigurationListener(BlockingConfigurationListener&&) = delete;
            BlockingConfigurationListener& operator=(BlockingConfigurationListener const&) = delete;
            BlockingConfigurationListener& operator=(BlockingConfigurationListener&&) = delete;
            ~BlockingConfigurationListener() = default;

            /*
             * configurationEvent is the method called by Configuration Admin whenever a
             * a configuraton object is updated or removed.
             *
             */
            void
            configurationEvent(cppmicroservices::service::cm::ConfigurationEvent const&) noexcept
            {
                {
                    std::lock_guard<std::mutex> lg { mutex_ };
                }
                promise_.set_value();
            }

            std::mutex mutex_;
            std::promise<void> promise_;
        };

        // The fixture for testing class ConfigurationAdminImpl.
        class TestConfigurationAdminImpl : public ::testing::Test
        {
          protected:
            TestConfigurationAdminImpl() : framework(cppmicroservices::FrameworkFactory().NewFramework()) {}

            ~TestConfigurationAdminImpl() override = default;

            void
            SetUp() override
            {
                framework.Start();
            }

            void
            TearDown() override
            {
                framework.Stop();
                framework.WaitForStop(std::chrono::milliseconds::zero());
            }

            cppmicroservices::Framework&
            GetFramework()
            {
                return framework;
            }

          protected:
            cppmicroservices::Framework framework;
        };

        /* The Update, UpdateIfDifferent and Remove methods on Configuration objects send
         * an asynchronous notification to all services that have published a ConfigurationListener
         * interface. The asynchronous thread creates a std::future and saves it in a vector (incompleteFutures).
         * A ConfigurationAdminImpl non-public method (WaitForAllAsync) can be used to wait for all futures
         * in the vector to complete. The future created by the asynchronous notification thread is
         * also shared (std::shared_future) and is returned to the
         * caller of the Update, UpdateIfDifferent or Remove method so they can wait for
         * the operation to complete. There was a bug in the initial version of ConfigurationAdmin
         * that resulted in a deadlock under some circumstances because a std::future was being saved
         * in the incompleteFutures vector instead of a std::shared_future. When the std::shared_future
         * went out of scope, the destructor would stall because the std::future would still exist. This test confirms
         * that this deadlock no longer exists.
         */
        TEST_F(TestConfigurationAdminImpl, VerifyNoUpdateDeadlock)
        {
            auto blockingConfigurationListener = std::make_shared<BlockingConfigurationListener>();
            auto callCompletedFut = blockingConfigurationListener->promise_.get_future();
            auto bundleContext = GetFramework().GetBundleContext();

            auto configListenerReg
                = bundleContext.RegisterService<cppmicroservices::service::cm::ConfigurationListener>(
                    blockingConfigurationListener);
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            auto configuration = configAdmin.GetConfiguration("testPid");
            cppmicroservices::AnyMap props(cppmicroservices::AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
            const std::string instanceId { "instance1" };
            props["uniqueProp"] = instanceId;

            {
                std::lock_guard<std::mutex> lg { blockingConfigurationListener->mutex_ };

                {
                    auto fut = configuration->Update(props);
                    EXPECT_EQ(std::future_status::timeout, fut.wait_for(std::chrono::milliseconds(10)));
                }
                // fut goes out of scope here. If the destructor blocks, the test would stall now.
            }
            // Lock is out of scope here, so configurationEvent should be unblocked, and the promise should be set.
            callCompletedFut.get();
        }

        TEST_F(TestConfigurationAdminImpl, VerifyGetConfiguration)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            {
                auto const conf = configAdmin.GetConfiguration("test.pid");
                ASSERT_TRUE(conf);
                EXPECT_EQ(conf->GetPid(), "test.pid");
                EXPECT_EQ(conf->GetFactoryPid(), "");
                auto props = conf->GetProperties();
                EXPECT_THAT(props, testing::SizeIs(0));
                props["foo"] = std::string { "bar" };
                EXPECT_NO_THROW(conf->Update(props));

                // Multiple requests should return the same object
                auto const conf2 = configAdmin.GetConfiguration("test.pid");
                EXPECT_EQ(conf, conf2);
                EXPECT_THAT(conf2->GetProperties(), testing::SizeIs(1));
            }

            // Same test with a Factory PID
            {
                auto const factoryConf = configAdmin.GetConfiguration("factory~instance1");
                ASSERT_TRUE(factoryConf);
                EXPECT_EQ(factoryConf->GetPid(), "factory~instance1");
                EXPECT_EQ(factoryConf->GetFactoryPid(), "factory");
                EXPECT_THAT(factoryConf->GetProperties(), testing::SizeIs(0));

                auto const factoryConf2 = configAdmin.GetConfiguration("factory~instance1");
                EXPECT_EQ(factoryConf, factoryConf2);
            }

            // Test GetConfiguration after removal
            auto const conf = configAdmin.GetConfiguration("test.pid");
            ASSERT_TRUE(conf);
            EXPECT_NO_THROW(conf->Remove());

            auto const conf2 = configAdmin.GetConfiguration("test.pid");
            ASSERT_TRUE(conf2);
            EXPECT_EQ(conf2->GetPid(), "test.pid");
            EXPECT_NE(conf, conf2);
        }

        TEST_F(TestConfigurationAdminImpl, VerifyCreateFactoryConfiguration)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            auto const conf = configAdmin.CreateFactoryConfiguration("factory");
            ASSERT_TRUE(conf);
            EXPECT_THAT(conf->GetPid(), testing::HasSubstr("factory~"));
            EXPECT_EQ(conf->GetFactoryPid(), "factory");
            EXPECT_THAT(conf->GetProperties(), testing::SizeIs(0));

            auto const conf2 = configAdmin.CreateFactoryConfiguration("factory");
            ASSERT_TRUE(conf2);
            EXPECT_THAT(conf2->GetPid(), testing::HasSubstr("factory~"));
            EXPECT_EQ(conf2->GetFactoryPid(), "factory");
            EXPECT_THAT(conf2->GetProperties(), testing::SizeIs(0));

            // Configurations should differ and have unique PIDs.
            EXPECT_NE(conf, conf2);
            EXPECT_NE(conf->GetPid(), conf2->GetPid());
        }

        TEST_F(TestConfigurationAdminImpl, VerifyGetFactoryConfiguration)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            {
                auto const conf = configAdmin.GetFactoryConfiguration("factory", "instance1");
                ASSERT_TRUE(conf);
                EXPECT_EQ(conf->GetPid(), "factory~instance1");
                EXPECT_EQ(conf->GetFactoryPid(), "factory");
                auto props = conf->GetProperties();
                EXPECT_THAT(props, testing::SizeIs(0));
                props["foo"] = std::string { "bar" };
                EXPECT_NO_THROW(conf->Update(props));

                // Should return a different Configuration
                auto const conf2 = configAdmin.GetFactoryConfiguration("factory", "instance2");
                ASSERT_TRUE(conf2);
                EXPECT_EQ(conf2->GetPid(), "factory~instance2");
                EXPECT_EQ(conf2->GetFactoryPid(), "factory");
                EXPECT_THAT(conf2->GetProperties(), testing::SizeIs(0));

                EXPECT_NE(conf, conf2);
            }

            // Should still return the original Configuration even though it is now out-of-scope.
            auto const conf3 = configAdmin.GetFactoryConfiguration("factory", "instance1");
            ASSERT_TRUE(conf3);
            EXPECT_EQ(conf3->GetFactoryPid(), "factory");
            EXPECT_EQ(conf3->GetPid(), "factory~instance1");
            EXPECT_THAT(conf3->GetProperties(), testing::SizeIs(1));
        }

        TEST_F(TestConfigurationAdminImpl, VerifyListConfigurations)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            const std::string pid1 { "test.pid1" };
            const std::string pid2 { "test.pid2" };

            auto const conf1 = configAdmin.GetConfiguration(pid1);
            auto const conf2 = configAdmin.GetConfiguration(pid2);

            auto props1 = conf1->GetProperties();
            props1["foo"] = std::string { "baz" };
            EXPECT_NO_THROW(conf1->Update(props1).get());

            auto props2 = conf2->GetProperties();
            props2["foo"] = std::string { "bar" };
            EXPECT_NO_THROW(conf2->Update(props2).get());

            auto const res1 = configAdmin.ListConfigurations();
            auto const res2 = configAdmin.ListConfigurations("(foo=bar)");
            auto const res3 = configAdmin.ListConfigurations("(foobar=baz)");

            EXPECT_EQ(res1.size(), 2ul);
            EXPECT_EQ(res2.size(), 1ul);
            EXPECT_EQ(res2[0]->GetPid(), pid2);
            EXPECT_TRUE(res3.empty());

            // ListConfigurations can return empty config objects (those with no properties) but
            // only if the configuration has been updated at least once. Nothing should
            // be returned here because the configuration object has not been updated.
            auto const emptyConfig = configAdmin.GetConfiguration("test.pid.emptyconfig");
            auto const emptyConfigResult = configAdmin.ListConfigurations("(pid=test.pid.emptyconfig)");
            EXPECT_TRUE(emptyConfigResult.empty());

            // ListConfigurations can return empty config objects (those with no properties) but
            // only if the configuration has been updated at least once. In this example,
            // only two configurations have been updated at least once.
            auto const allConfigsResult = configAdmin.ListConfigurations();
            EXPECT_EQ(allConfigsResult.size(), 2ul);

            // ListConfigurations should return empty config objects if the config
            // object was defined in a manifest.json file and added using AddConfigurations.
            // Adding a configuration object this way counts as a create and update operation.
            std::vector<metadata::ConfigurationMetadata> configs;
            configs.push_back(
                metadata::ConfigurationMetadata("test.pid3", AnyMap { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS }));
            auto result = configAdmin.AddConfigurations(std::move(configs));
            auto allConfigs = configAdmin.ListConfigurations();
            EXPECT_EQ(allConfigs.size(), 3ul);

            // If the properties for a configuration object are removed, ListConfigurations
            // should still include that configuration object in the result list.
            cppmicroservices::AnyMap props(cppmicroservices::AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
            EXPECT_NO_THROW(conf1->Update(props).get());
            allConfigs = configAdmin.ListConfigurations();
            EXPECT_EQ(allConfigs.size(), 3ul);
        }

        TEST_F(TestConfigurationAdminImpl, VerifyAddConfigurations)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            // Set up some existing Configurations
            auto const conf = configAdmin.GetConfiguration("test.pid");
            auto const conf2 = configAdmin.GetConfiguration("test.pid2");
            ASSERT_TRUE(conf);
            ASSERT_TRUE(conf2);
            auto props = conf2->GetProperties();
            props["foo"] = std::string { "bar" };
            std::shared_future<void> fut;
            EXPECT_NO_THROW(fut = conf2->Update(props));
            fut.get();
            std::vector<metadata::ConfigurationMetadata> configs;

            configs.push_back(
                metadata::ConfigurationMetadata("test.pid", AnyMap { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS }));

            AnyMap props2 { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            props2["foo"] = std::string { "baz" };
            configs.push_back(metadata::ConfigurationMetadata("test.pid2", props2));

            AnyMap props3 { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            props3["bar"] = std::string { "baz" };
            configs.push_back(metadata::ConfigurationMetadata("test.pid3", props3));

            auto result = configAdmin.AddConfigurations(std::move(configs));

            decltype(result) expected {
                { "test.pid", 0ul,  reinterpret_cast<std::uintptr_t>(conf.get())},
                {"test.pid2", 2ul, reinterpret_cast<std::uintptr_t>(conf2.get())}
            };

            ASSERT_THAT(result, testing::SizeIs(3));
            EXPECT_EQ(result[0], expected[0]);
            EXPECT_EQ(result[1], expected[1]);

            EXPECT_EQ(result[2].pid, "test.pid3");
            EXPECT_EQ(result[2].changeCount, 1ul);
            EXPECT_NE(result[2].configurationId, reinterpret_cast<std::uintptr_t>(conf.get()));
            EXPECT_NE(result[2].configurationId, reinterpret_cast<std::uintptr_t>(conf2.get()));

            props = conf2->GetProperties();
            EXPECT_THAT(props, AnyMapEquals(props2));

            auto conf3 = configAdmin.GetConfiguration("test.pid3");
            ASSERT_TRUE(conf3);
            props = conf3->GetProperties();
            EXPECT_THAT(props, AnyMapEquals(props3));
        }

        TEST_F(TestConfigurationAdminImpl, VerifyRemoveConfigurations)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            // Set up some existing Configurations
            auto const conf = configAdmin.GetConfiguration("test.pid");
            auto const conf2 = configAdmin.GetConfiguration("test.pid2");

            std::vector<metadata::ConfigurationMetadata> configs;

            configs.push_back(
                metadata::ConfigurationMetadata("test.pid", AnyMap { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS }));

            AnyMap props2 { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            props2["foo"] = std::string { "baz" };
            configs.push_back(metadata::ConfigurationMetadata("test.pid2", props2));

            AnyMap props3 { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            props3["bar"] = std::string { "baz" };
            configs.push_back(metadata::ConfigurationMetadata("test.pid3", props3));

            // The test.pid Configuration shouldn't have been modified, so should be removed.
            // Remove pid2, Update pid3, Remove & re-add pid4, so they too should be left alone.
            // Leave pid5 unchanged, so it should be Removed.

            AnyMap props4 { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            props4["baz"] = std::string { "foo" };
            configs.push_back(metadata::ConfigurationMetadata("test.pid4", props4));

            configs.push_back(
                metadata::ConfigurationMetadata("test.pid5", AnyMap { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS }));

            auto result = configAdmin.AddConfigurations(std::move(configs));
            auto const conf3 = configAdmin.GetConfiguration("test.pid3");
            auto conf4 = configAdmin.GetConfiguration("test.pid4");
            auto const conf5 = configAdmin.GetConfiguration("test.pid5");

            ASSERT_TRUE(conf);
            ASSERT_TRUE(conf2);
            ASSERT_TRUE(conf3);
            ASSERT_TRUE(conf4);
            ASSERT_TRUE(conf5);

            EXPECT_NO_THROW(conf2->Remove());
            props3["bar"] = std::string { "foo" };
            EXPECT_NO_THROW(conf3->Update(props3));
            EXPECT_NO_THROW(conf4->Remove());
            conf4 = configAdmin.GetConfiguration("test.pid4");

            EXPECT_NO_THROW(configAdmin.RemoveConfigurations(std::move(result)));

            EXPECT_NE(conf, configAdmin.GetConfiguration("test.pid"));
            EXPECT_NE(conf2, configAdmin.GetConfiguration("test.pid2"));
            EXPECT_EQ(conf3, configAdmin.GetConfiguration("test.pid3"));
            EXPECT_EQ(conf4, configAdmin.GetConfiguration("test.pid4"));
            EXPECT_NE(conf5, configAdmin.GetConfiguration("test.pid5"));
        }

        TEST_F(TestConfigurationAdminImpl, VerifyManagedServiceNotification)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            // Set up an existing Configuration
            auto const conf = configAdmin.GetConfiguration("test.pid");
            ASSERT_TRUE(conf);
            auto props = conf->GetProperties();
            props["foo"] = std::string { "bar" };
            EXPECT_NO_THROW(conf->Update(props));

            AnyMap emptyProps { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };

            std::mutex counterMutex;
            std::condition_variable counterCV;
            auto msCounter = 0u;
            auto ms2Counter = 0u;

            auto f = [&counterMutex, &counterCV, &msCounter]
            {
                {
                    std::lock_guard<std::mutex> lk { counterMutex };
                    ++msCounter;
                }
                counterCV.notify_one();
            };

            auto newProps = props;
            newProps["foo"] = std::string { "baz" };

            auto mockManagedService = std::make_shared<MockManagedService>();
            auto mockManagedService2 = std::make_shared<MockManagedService>();
            auto mockManagedService3 = std::make_shared<MockManagedService>();
            // setup expectations.
            EXPECT_CALL(*mockManagedService, Updated(AnyMapEquals(props))).WillOnce(testing::InvokeWithoutArgs(f));
            EXPECT_CALL(*mockManagedService, Updated(AnyMapEquals(newProps))).WillOnce(testing::InvokeWithoutArgs(f));
            EXPECT_CALL(*mockManagedService2, Updated(testing::_)).Times(0);
            EXPECT_CALL(*mockManagedService3, Updated(testing::_)).Times(0);

            // Ensure notification from original GetConfiguration has run.
            configAdmin.WaitForAllAsync();

            auto ms1Props = cppmicroservices::ServiceProperties({
                {std::string("service.pid"), std::string("test.pid")}
            });
            auto ms2Props = cppmicroservices::ServiceProperties({
                {std::string("component.name"), std::string("test.pid2")}
            });

            auto reg1 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedService>(mockManagedService,
                                                                                                     ms1Props);
            auto reg2
                = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedService>(mockManagedService2,
                                                                                               ms2Props);
            auto reg3
                = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedService>(mockManagedService3);

            std::unique_lock<std::mutex> ul { counterMutex };
            auto invokedOnce
                = counterCV.wait_for(ul, std::chrono::seconds(10), [&msCounter] { return 1u == msCounter; });
            ul.unlock();

            EXPECT_TRUE(invokedOnce);
            EXPECT_NO_THROW(conf->Update(newProps));

            ul.lock();
            auto invokeComplete
                = counterCV.wait_for(ul,
                                     std::chrono::seconds(10),
                                     [&msCounter, &ms2Counter] { return 2u == msCounter && 0u == ms2Counter; });
            ul.unlock();

            EXPECT_TRUE(invokeComplete);

            reg1.Unregister();
            reg2.Unregister();
            reg3.Unregister();

            configAdmin.WaitForAllAsync();
        }

        TEST_F(TestConfigurationAdminImpl, VerifyManagedServiceFactoryNotification)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();
            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

            // Set up an existing Configuration
            auto const conf = configAdmin.GetConfiguration("factory2~instance1");
            ASSERT_TRUE(conf);
            auto props = conf->GetProperties();
            props["foo"] = std::string { "bar" };
            EXPECT_NO_THROW({
                auto fut = conf->Update(props);
                fut.get();
            });

            AnyMap emptyProps { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };

            std::mutex counterMutex;
            std::condition_variable counterCV;
            auto msCounter = 0u;
            auto ms2Counter = 0u;

            auto f = [&counterMutex, &counterCV, &msCounter]
            {
                {
                    std::lock_guard<std::mutex> lk { counterMutex };
                    ++msCounter;
                }
                counterCV.notify_one();
            };

            auto f2 = [&counterMutex, &counterCV, &ms2Counter]
            {
                {
                    std::lock_guard<std::mutex> lk { counterMutex };
                    ++ms2Counter;
                }
                counterCV.notify_one();
            };

            auto newProps = props;
            newProps["foo"] = std::string { "baz" };

            auto mockManagedServiceFactory = std::make_shared<MockManagedServiceFactory>();
            auto mockManagedServiceFactory2 = std::make_shared<MockManagedServiceFactory>();
            auto mockManagedServiceFactory3 = std::make_shared<MockManagedServiceFactory>();
            // setup expectations.
            // mockManagedServiceFactory will receive one Updated notification
            // and one Removed Notification.
            // mockManagedServiceFactory2 will receive two Updated notifications.
            // mockManagedServiceFactory3 will receive no notifications.
            EXPECT_CALL(*mockManagedServiceFactory, Updated(std::string { "factory~instance1" }, AnyMapEquals(props)))
                .Times(1);
            EXPECT_CALL(*mockManagedServiceFactory2, Updated(std::string { "factory2~instance1" }, AnyMapEquals(props)))
                .WillOnce(testing::InvokeWithoutArgs(f2));
            EXPECT_CALL(*mockManagedServiceFactory,
                        Updated(std::string { "factory~instance1" }, AnyMapEquals(emptyProps)))
                .Times(0);
            EXPECT_CALL(*mockManagedServiceFactory,
                        Updated(std::string { "factory~instance2" }, AnyMapEquals(emptyProps)))
                .Times(0);
            EXPECT_CALL(*mockManagedServiceFactory2,
                        Updated(std::string { "factory2~instance1" }, AnyMapEquals(newProps)))
                .WillOnce(testing::InvokeWithoutArgs(f2));
            EXPECT_CALL(*mockManagedServiceFactory, Removed(std::string { "factory~instance1" }))
                .WillOnce(testing::InvokeWithoutArgs(f));
            EXPECT_CALL(*mockManagedServiceFactory3, Updated(testing::_, testing::_)).Times(0);
            EXPECT_CALL(*mockManagedServiceFactory3, Removed(testing::_)).Times(0);

            AnyMap pidProp { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            pidProp["pid"] = std::string("factory");
            AnyMap nameProp { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            nameProp["name"] = std::string("factory2");
            auto ms1Props = cppmicroservices::ServiceProperties({
                {std::string("service"), pidProp}
            });
            auto ms2Props = cppmicroservices::ServiceProperties({
                {std::string("component"), nameProp}
            });

            auto reg1 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedServiceFactory>(
                mockManagedServiceFactory,
                ms1Props);
            auto reg2 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedServiceFactory>(
                mockManagedServiceFactory2,
                ms2Props);
            auto reg3 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedServiceFactory>(
                mockManagedServiceFactory3);

            std::unique_lock<std::mutex> ul { counterMutex };
            auto factory2InvokedOnce
                = counterCV.wait_for(ul, std::chrono::seconds(10), [&ms2Counter] { return 1u == ms2Counter; });
            ul.unlock();

            EXPECT_TRUE(factory2InvokedOnce);
            configAdmin.WaitForAllAsync();

            auto conf2 = configAdmin.GetConfiguration("factory~instance1");
            auto conf3 = configAdmin.GetConfiguration("factory~instance2");
            ASSERT_TRUE(conf2);
            ASSERT_TRUE(conf3);

            ul.lock();
            auto factory1InvokedZeroTimes
                = counterCV.wait_for(ul, std::chrono::seconds(10), [&msCounter] { return 0u == msCounter; });
            ul.unlock();

            EXPECT_TRUE(factory1InvokedZeroTimes);
            EXPECT_NO_THROW({
                auto result = conf->Update(newProps);
                result.get();
            });
            EXPECT_NO_THROW({
                auto result1 = conf2->Update(props);
                result1.get();
            });
            EXPECT_NO_THROW({
                auto result2 = conf2->Remove();
                result2.get();
            });

            ul.lock();
            auto invokeComplete
                = counterCV.wait_for(ul,
                                     std::chrono::seconds(10),
                                     [&msCounter, &ms2Counter] { return 1u == msCounter && 2u == ms2Counter; });
            ul.unlock();

            EXPECT_TRUE(invokeComplete);

            reg1.Unregister();
            reg2.Unregister();
            reg3.Unregister();

            configAdmin.WaitForAllAsync();
        }
        // This test confirms that when ConfigurationAdmin shuts down the appropriate
        // Removed notifications should be sent to the ManagedService and ManagedFactoryServices.
        // Configuration objects will be added to the repository but never updated so when
        // ConfigurationAdmin shuts down no Remove notifications should be sent to the
        // ManagedServices or ManagedServiceFactorys
        TEST_F(TestConfigurationAdminImpl, VerifyConfigAdminStartupShutdownNotification)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();

            AnyMap emptyProps { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };

            std::mutex counterMutex;
            std::condition_variable counterCV;
            auto msCounter = 0u;
            auto msfCounter = 0u;

            auto mockManagedService = std::make_shared<MockManagedService>();
            auto mockManagedService2 = std::make_shared<MockManagedService>();
            auto mockManagedServiceFactory = std::make_shared<MockManagedServiceFactory>();
            auto mockManagedServiceFactory2 = std::make_shared<MockManagedServiceFactory>();
            // setup expectations.
            // Test registers two mockManagedServices and two mockManagedFactoryServices
            // Configuration objects are created but never updated so no Updated or Removed
            // notifications should be sent to the services when ConfigurationAdmin
            // shuts down.

            // ManagedServices do not have a Removed method. Updated is called with empty
            // properties for a Removed notification.
            EXPECT_CALL(*mockManagedService, Updated(AnyMapEquals(emptyProps))).Times(0);
            EXPECT_CALL(*mockManagedServiceFactory,
                        Updated(std::string { "factory~instance1" }, AnyMapEquals(emptyProps)))
                .Times(0);
            EXPECT_CALL(*mockManagedServiceFactory, Removed(std::string { "factory~instance1" })).Times(0);
            EXPECT_CALL(*mockManagedService2, Updated(testing::_)).Times(0);
            EXPECT_CALL(*mockManagedServiceFactory2, Updated(testing::_, testing::_)).Times(0);
            EXPECT_CALL(*mockManagedServiceFactory2, Removed(testing::_)).Times(0);

            auto msProps = cppmicroservices::ServiceProperties({
                {std::string("service.pid"), std::string("test.pid")}
            });
            auto msfProps = cppmicroservices::ServiceProperties({
                {std::string("service.pid"), std::string("factory")}
            });

            auto reg1 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedServiceFactory>(
                mockManagedServiceFactory,
                msfProps);
            auto reg2 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedServiceFactory>(
                mockManagedServiceFactory2);
            auto reg3 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedService>(mockManagedService,
                                                                                                     msProps);
            auto reg4
                = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedService>(mockManagedService2);

            {
                std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                    = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
                ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

                std::unique_lock<std::mutex> ul { counterMutex };
                auto invokedZeroTimes
                    = counterCV.wait_for(ul, std::chrono::seconds(10), [&msCounter] { return 0u == msCounter; });
                ul.unlock();

                EXPECT_TRUE(invokedZeroTimes);
                configAdmin.WaitForAllAsync();

                auto conf = configAdmin.GetConfiguration("factory~instance1");
                ASSERT_TRUE(conf);

                ul.lock();
                auto factoryInvokedZeroTimes
                    = counterCV.wait_for(ul, std::chrono::seconds(10), [&msfCounter] { return 0u == msfCounter; });
                ul.unlock();

                EXPECT_TRUE(factoryInvokedZeroTimes);
                // ConfigurationAdminImpl was constructed in this scope so the next statement will
                // cause it to be shut down.
                // The configuration objects that were added to the repository were never updated
                // so no Removed notifications will be sent to the ManagedServices or the
                // ManagedFactoryServices. Removed notifications can only be sent if an Updated
                // notification has been previously sent.
            }

            std::unique_lock<std::mutex> ul { counterMutex };
            auto invokeComplete
                = counterCV.wait_for(ul,
                                     std::chrono::seconds(10),
                                     [&msCounter, &msfCounter] { return 0u == msCounter && 0u == msfCounter; });
            ul.unlock();

            EXPECT_TRUE(invokeComplete);

            reg1.Unregister();
            reg2.Unregister();
            reg3.Unregister();
            reg4.Unregister();
        }
        // This test confirms that when ConfigurationAdmin shuts down the appropriate
        // Removed notifications are sent to the ManagedService and ManagedServiceFactories
        // Configuration objects will be added to the repository and updated so when
        // ConfigurationAdmin shuts down  Remove notifications should be sent to the
        // ManagedServices and ManagedServiceFactories
        TEST_F(TestConfigurationAdminImpl, VerifyConfigAdminStartupShutdownNotificationWithUpdate)
        {
            auto bundleContext = GetFramework().GetBundleContext();
            auto fakeLogger = std::make_shared<FakeLogger>();

            AnyMap emptyProps { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };
            cppmicroservices::AnyMap props(cppmicroservices::AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
            const std::string instanceId { "instance1" };
            props["uniqueProp"] = instanceId;

            std::mutex counterMutex;
            std::condition_variable counterCV;
            auto msCounter = 0u;
            auto msfCounter = 0u;

            auto f = [&counterMutex, &counterCV, &msCounter]
            {
                {
                    std::lock_guard<std::mutex> lk { counterMutex };
                    ++msCounter;
                }
                counterCV.notify_one();
            };
            auto f2 = [&counterMutex, &counterCV, &msfCounter]
            {
                {
                    std::lock_guard<std::mutex> lk { counterMutex };
                    ++msfCounter;
                }
                counterCV.notify_one();
            };

            auto mockManagedService = std::make_shared<MockManagedService>();
            auto mockManagedServiceFactory = std::make_shared<MockManagedServiceFactory>();
            // setup expectations.
            // The test registers a mockManagedService and a mockManagedFactoryService.
            // Configuration objects are created and updated so each of the services
            // should receive one Updated notification and one Removed notification.

            // For the mockManagedService the Updated method is used for both Updated
            // and Removed. The Removed notification calls Updated with empty properties.
            EXPECT_CALL(*mockManagedService, Updated(AnyMapEquals(props))).Times(1);
            EXPECT_CALL(*mockManagedService, Updated(AnyMapEquals(emptyProps))).WillOnce(testing::InvokeWithoutArgs(f));
            EXPECT_CALL(*mockManagedServiceFactory, Updated(std::string { "factory~instance1" }, AnyMapEquals(props)))
                .Times(1);
            EXPECT_CALL(*mockManagedServiceFactory, Removed(std::string { "factory~instance1" }))
                .WillOnce(testing::InvokeWithoutArgs(f2));

            auto msProps = cppmicroservices::ServiceProperties({
                {std::string("service.pid"), std::string("test.pid")}
            });
            auto msfProps = cppmicroservices::ServiceProperties({
                {std::string("service.pid"), std::string("factory")}
            });

            auto reg1 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedServiceFactory>(
                mockManagedServiceFactory,
                msfProps);
            auto reg2 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedService>(mockManagedService,
                                                                                                     msProps);

            {
                std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                    = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, fakeLogger);
                ConfigurationAdminImpl configAdmin(bundleContext, fakeLogger, asyncWorkService);

                auto config = configAdmin.GetConfiguration("test.pid");
                ASSERT_TRUE(config);
                auto fut = config->Update(props);
                fut.get();

                std::unique_lock<std::mutex> ul { counterMutex };
                auto invokedZeroTimes
                    = counterCV.wait_for(ul, std::chrono::seconds(10), [&msCounter] { return 0u == msCounter; });
                ul.unlock();

                EXPECT_TRUE(invokedZeroTimes);

                auto conf = configAdmin.GetConfiguration("factory~instance1");
                ASSERT_TRUE(conf);
                fut = conf->Update(props);
                fut.get();

                ul.lock();
                auto factoryInvokedZeroTimes
                    = counterCV.wait_for(ul, std::chrono::seconds(10), [&msfCounter] { return 0u == msfCounter; });
                ul.unlock();

                EXPECT_TRUE(factoryInvokedZeroTimes);
                // ConfigurationAdminImpl was constructed in this scope so the next statement will
                // cause it to be shut down.
                // The configuration objects that were added to the repository were updated
                // so Removed notifications will be sent to the ManagedServices and the
                // ManagedFactoryServices. Removed notifications must be sent if an Updated
                // notification has been previously sent.
            }

            std::unique_lock<std::mutex> ul { counterMutex };
            auto invokeComplete
                = counterCV.wait_for(ul,
                                     std::chrono::seconds(10),
                                     [&msCounter, &msfCounter] { return 1u == msCounter && 1u == msfCounter; });
            ul.unlock();

            EXPECT_TRUE(invokeComplete);

            reg1.Unregister();
            reg2.Unregister();
        }
        TEST_F(TestConfigurationAdminImpl, VerifyManagedServiceExceptionsAreLogged)
        {
            using cppmicroservices::logservice::SeverityLevel;
            using cppmicroservices::service::cm::ConfigurationException;

            auto bundleContext = GetFramework().GetBundleContext();
            auto mockLogger = std::make_shared<MockLogger>();

            // set logging expectations
            auto ConfigurationExceptionThrownByManagedService
                = testing::AllOf(testing::HasSubstr("ConfigurationException thrown by ManagedService with "
                                                    "PID test.pid whilst being Updated with new properties"),
                                 testing::HasSubstr("The property which caused this error was 'foo'"),
                                 testing::HasSubstr("Exception reason: A reason"));
            auto ExceptionThrownByManagedService
                = testing::AllOf(testing::HasSubstr("Exception thrown by ManagedService with PID test.pid "
                                                    "whilst being Updated with new properties"),
                                 testing::HasSubstr("Exception: An exception"));
            auto UnknownExceptionThrownByManagedService
                = testing::HasSubstr("Unknown exception thrown by ManagedService with PID "
                                     "test.pid whilst being Updated with new properties");

            auto ExceptionThrownByManagedServiceFactoryDuringUpdate
                = testing::AllOf(testing::HasSubstr("Exception thrown by ManagedServiceFactory with PID factory~"),
                                 testing::HasSubstr("whilst being Updated with new properties"),
                                 testing::HasSubstr("Exception: An exception"));
            auto UnknownExceptionThrownByManagedServiceFactoryDuringUpdate = testing::AllOf(
                testing::HasSubstr("Unknown exception thrown by ManagedServiceFactory with PID factory~"),
                testing::HasSubstr("whilst being Updated with new properties"));

            auto ExceptionThrownByManagedServiceFactoryDuringRemoval
                = testing::AllOf(testing::HasSubstr("Exception thrown by ManagedServiceFactory with PID factory~"),
                                 testing::HasSubstr("whilst being notified of Configuration Removal."),
                                 testing::HasSubstr("Exception: An exception"));
            auto UnknownExceptionThrownByManagedServiceFactoryDuringRemoval = testing::AllOf(
                testing::HasSubstr("Unknown exception thrown by ManagedServiceFactory with PID factory~"),
                testing::HasSubstr("whilst being notified of Configuration Removal."));

            EXPECT_CALL(*mockLogger, Log(SeverityLevel::LOG_DEBUG, testing::_)).Times(testing::AtLeast(1));
            EXPECT_CALL(*mockLogger, Log(SeverityLevel::LOG_ERROR, ConfigurationExceptionThrownByManagedService))
                .Times(1);
            EXPECT_CALL(*mockLogger, Log(SeverityLevel::LOG_ERROR, ExceptionThrownByManagedService)).Times(1);
            EXPECT_CALL(*mockLogger, Log(SeverityLevel::LOG_ERROR, UnknownExceptionThrownByManagedService)).Times(1);
            EXPECT_CALL(*mockLogger, Log(SeverityLevel::LOG_ERROR, ExceptionThrownByManagedServiceFactoryDuringUpdate))
                .Times(1);
            EXPECT_CALL(*mockLogger,
                        Log(SeverityLevel::LOG_ERROR, UnknownExceptionThrownByManagedServiceFactoryDuringUpdate))
                .Times(1);
            EXPECT_CALL(*mockLogger, Log(SeverityLevel::LOG_ERROR, ExceptionThrownByManagedServiceFactoryDuringRemoval))
                .Times(1);
            EXPECT_CALL(*mockLogger,
                        Log(SeverityLevel::LOG_ERROR, UnknownExceptionThrownByManagedServiceFactoryDuringRemoval))
                .Times(1);

            std::shared_ptr<cppmicroservices::cmimpl::CMAsyncWorkService> asyncWorkService
                = std::make_shared<cppmicroservices::cmimpl::CMAsyncWorkService>(bundleContext, mockLogger);
            ConfigurationAdminImpl configAdmin(bundleContext, mockLogger, asyncWorkService);

            // Set up an existing Configuration
            auto const conf = configAdmin.GetConfiguration("test.pid");
            ASSERT_TRUE(conf);
            cppmicroservices::AnyMap props(cppmicroservices::AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
            const std::string instanceId { "instance1" };
            props["uniqueProp"] = instanceId;
            conf->Update(props);

            AnyMap emptyProps { AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS };

            std::mutex counterMutex;
            std::condition_variable counterCV;
            auto msCounter = 0u;
            auto msfCounter = 0u;

            auto f = [&counterMutex, &counterCV, &msCounter]
            {
                {
                    std::lock_guard<std::mutex> lk { counterMutex };
                    ++msCounter;
                }
                counterCV.notify_one();
            };

            auto f2 = [&counterMutex, &counterCV, &msfCounter]
            {
                {
                    std::lock_guard<std::mutex> lk { counterMutex };
                    ++msfCounter;
                }
                counterCV.notify_one();
            };

            auto mockManagedService = std::make_shared<MockManagedService>();
            auto mockManagedServiceFactory = std::make_shared<MockManagedServiceFactory>();
            // setup expectations.
            // Updated will be called once when the service is registered and once
            // when Update is called after the service is registered.
            EXPECT_CALL(*mockManagedService, Updated(AnyMapEquals(props)))
                .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(f),
                                         testing::Throw(ConfigurationException("A reason", "foo"))))
                .WillOnce(
                    testing::DoAll(testing::InvokeWithoutArgs(f), testing::Throw(std::runtime_error("An exception"))));

            // Update is called without any properties for a Remove operation
            EXPECT_CALL(*mockManagedService, Updated(AnyMapEquals(emptyProps)))
                .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(f), testing::Throw(42)));

            EXPECT_CALL(*mockManagedServiceFactory, Updated(testing::HasSubstr("factory~"), AnyMapEquals(props)))
                .WillOnce(
                    testing::DoAll(testing::InvokeWithoutArgs(f2), testing::Throw(std::runtime_error("An exception"))))
                .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(f2), testing::Throw(42)));

            EXPECT_CALL(*mockManagedServiceFactory, Removed(testing::HasSubstr("factory~")))
                .WillOnce(
                    testing::DoAll(testing::InvokeWithoutArgs(f2), testing::Throw(std::runtime_error("An exception"))))
                .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(f2), testing::Throw(42)));

            // Ensure notification from original GetConfiguration has run.
            configAdmin.WaitForAllAsync();

            auto msProps = cppmicroservices::ServiceProperties({
                {std::string("service.pid"), std::string("test.pid")}
            });
            auto msfProps = cppmicroservices::ServiceProperties({
                {std::string("service.pid"), std::string("factory")}
            });

            auto reg1 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedServiceFactory>(
                mockManagedServiceFactory,
                msfProps);
            auto reg2 = bundleContext.RegisterService<cppmicroservices::service::cm::ManagedService>(mockManagedService,
                                                                                                     msProps);

            std::unique_lock<std::mutex> ul { counterMutex };
            auto invokedOnce
                = counterCV.wait_for(ul, std::chrono::seconds(10), [&msCounter] { return 1u == msCounter; });
            ul.unlock();

            EXPECT_TRUE(invokedOnce);
            configAdmin.WaitForAllAsync();

            EXPECT_NO_THROW({
                auto fut = conf->Update(props);
                fut.get();
            });
            EXPECT_NO_THROW(conf->Remove());

            auto factoryConf = configAdmin.CreateFactoryConfiguration("factory");
            auto factoryConf2 = configAdmin.CreateFactoryConfiguration("factory");

            EXPECT_NO_THROW({
                auto result = factoryConf->Update(props);
                result.get();
            });
            EXPECT_NO_THROW({
                auto result2 = factoryConf2->Update(props);
                result2.get();
            });

            ul.lock();
            auto factoryInvokedTwice
                = counterCV.wait_for(ul, std::chrono::seconds(10), [&msfCounter] { return 2u == msfCounter; });
            US_UNUSED(factoryInvokedTwice);
            ul.unlock();

            EXPECT_NO_THROW({
                auto result = factoryConf->Remove();
                result.get();
            });
            EXPECT_NO_THROW({
                auto result2 = factoryConf2->Remove();
                result2.get();
            });

            ul.lock();
            auto invokeComplete
                = counterCV.wait_for(ul,
                                     std::chrono::seconds(10),
                                     [&msCounter, &msfCounter] { return 3u == msCounter && 4u == msfCounter; });
            ul.unlock();

            EXPECT_TRUE(invokeComplete);

            reg1.Unregister();
            reg2.Unregister();

            configAdmin.WaitForAllAsync();
        }
    } // namespace cmimpl
} // namespace cppmicroservices
