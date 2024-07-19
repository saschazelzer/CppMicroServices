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

#include "gtest/gtest.h"

#include <cppmicroservices/Bundle.h>
#include <cppmicroservices/BundleContext.h>
#include <cppmicroservices/BundleEvent.h>
#include <cppmicroservices/Framework.h>
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/FrameworkFactory.h>

#include "../TestUtils.hpp"
#include "ConcurrencyTestUtil.hpp"
#include "Mocks.hpp"
#include "TestFixture.hpp"
#include "TestInterfaces/Interfaces.hpp"
#include "boost/asio/async_result.hpp"
#include "boost/asio/packaged_task.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"
#include "cppmicroservices/servicecomponent/ComponentConstants.hpp"
#include "cppmicroservices/servicecomponent/runtime/ServiceComponentRuntime.hpp"

#include <future>
#include <thread>

namespace test
{

    namespace async = cppmicroservices::async;
    namespace scr = cppmicroservices::service::component::runtime;

    class TestAsyncWorkServiceEndToEnd
        : public ::testing::TestWithParam<std::shared_ptr<cppmicroservices::async::AsyncWorkService>>
    {
      public:
        TestAsyncWorkServiceEndToEnd() : framework(cppmicroservices::FrameworkFactory().NewFramework()) {}

        void
        SetUp() override
        {
            framework.Start();
            auto context = framework.GetBundleContext();

            ::test::InstallAndStartDS(context);

            auto sRef = context.GetServiceReference<scr::ServiceComponentRuntime>();
            ASSERT_TRUE(sRef);
            dsRuntimeService = context.GetService<scr::ServiceComponentRuntime>(sRef);
            ASSERT_TRUE(dsRuntimeService);
        }
        void
        TearDown() override
        {
            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }

        std::shared_ptr<scr::ServiceComponentRuntime> dsRuntimeService;
        cppmicroservices::Framework framework;
    };

    class AsyncWorkServiceThreadPool : public cppmicroservices::async::AsyncWorkService
    {
      public:
        AsyncWorkServiceThreadPool(int nThreads) : cppmicroservices::async::AsyncWorkService()
        {
            threadpool = std::make_shared<boost::asio::thread_pool>(nThreads);
        }

        ~AsyncWorkServiceThreadPool() override
        {
            try
            {
                if (threadpool)
                {
                    try
                    {
                        threadpool->join();
                    }
                    catch (...)
                    {
                        //
                    }
                }
            }
            catch (...)
            {
                //
            }
        }

        void
        post(std::packaged_task<void()>&& task) override
        {
            using Sig = void();
            using Result = boost::asio::async_result<decltype(task), Sig>;
            using Handler = typename Result::completion_handler_type;

            Handler handler(std::forward<decltype(task)>(task));
            Result result(handler);

            boost::asio::post(threadpool->get_executor(), [handler = std::move(handler)]() mutable { handler(); });
        }

        void
        waitForAllPostedTasksToRun() override
        {
            try
            {
                if (threadpool)
                {
                    try
                    {
                        threadpool->join();
                    }
                    catch (...)
                    {
                        //
                    }
                }
            }
            catch (...)
            {
                //
            }
        }

      private:
        std::shared_ptr<boost::asio::thread_pool> threadpool;
    };

    TEST_F(tGenericDSSuite, TestAsyncWorkServiceWithoutUserService)
    {
        std::shared_ptr<cppmicroservices::scrimpl::SCRLogger> logger
            = std::make_shared<cppmicroservices::scrimpl::SCRLogger>(framework.GetBundleContext());

        // Create SCRAsyncWorkService, just make sure Adding service isn't called, look
        // at SCRLoggerTest for example
        cppmicroservices::scrimpl::SCRAsyncWorkService scrAsyncWorkService(framework.GetBundleContext(), logger);
        EXPECT_NO_THROW({
            std::packaged_task<void()> myTask(
                []()
                {
                    int v = 1 + 2;
                    US_UNUSED(v);
                });
            std::future<void> f = myTask.get_future();
            scrAsyncWorkService.post(std::move(myTask));
            f.get();
        });
    }

    TEST_F(tGenericDSSuite, TestUserServiceUsedAfterInstall)
    {
        EXPECT_NO_THROW({
            auto mockAsyncWorkService = std::make_shared<cppmicroservices::async::MockAsyncWorkService>();
            auto bundleContext = framework.GetBundleContext();
            auto reg = bundleContext.RegisterService<cppmicroservices::async::AsyncWorkService>(mockAsyncWorkService);
            EXPECT_CALL(*mockAsyncWorkService, post(::testing::_)).Times(1);

            std::shared_ptr<cppmicroservices::scrimpl::SCRLogger> logger
                = std::make_shared<cppmicroservices::scrimpl::SCRLogger>(framework.GetBundleContext());

            cppmicroservices::scrimpl::SCRAsyncWorkService scrAsyncWorkService(bundleContext, logger);

            std::packaged_task<void()> myTask(
                []()
                {
                    int v = 1 + 2;
                    US_UNUSED(v);
                });
            // We don't manage the future and wait because post is mocked and has no default behavior.
            scrAsyncWorkService.post(std::move(myTask));
        });
    }

    TEST_F(tGenericDSSuite, TestFallbackUsedAfterUnregister)
    {
        // don't hold onto the shared pointer to the service object, unregister, reset(), ...
        EXPECT_NO_THROW({
            auto mockAsyncWorkService = std::make_shared<cppmicroservices::async::MockAsyncWorkService>();
            auto bundleContext = framework.GetBundleContext();
            auto reg = bundleContext.RegisterService<cppmicroservices::async::AsyncWorkService>(mockAsyncWorkService);
            EXPECT_CALL(*mockAsyncWorkService, post(::testing::_)).Times(1);

            std::shared_ptr<cppmicroservices::scrimpl::SCRLogger> logger
                = std::make_shared<cppmicroservices::scrimpl::SCRLogger>(framework.GetBundleContext());

            cppmicroservices::scrimpl::SCRAsyncWorkService scrAsyncWorkService(bundleContext, logger);

            std::packaged_task<void()> myTask(
                []()
                {
                    int v = 1 + 2;
                    US_UNUSED(v);
                });
            // We don't manage the future and wait because post is mocked and has no default behavior.
            scrAsyncWorkService.post(std::move(myTask));

            reg.Unregister();

            EXPECT_CALL(*mockAsyncWorkService, post(::testing::_)).Times(0);

            std::packaged_task<void()> myTask2(
                []()
                {
                    int v = 1 + 2;
                    US_UNUSED(v);
                });
            scrAsyncWorkService.post(std::move(myTask2));
        });
    }

    TEST_F(tGenericDSSuite, TestUseAsyncWorkServiceDuringConcurrentBundleOperations)
    {
        EXPECT_NO_THROW({
            auto bundleContext = framework.GetBundleContext();

            std::shared_ptr<cppmicroservices::scrimpl::SCRLogger> logger
                = std::make_shared<cppmicroservices::scrimpl::SCRLogger>(bundleContext);

            cppmicroservices::scrimpl::SCRAsyncWorkService scrAsyncWorkService(bundleContext, logger);
            std::promise<void> startPromise;
            std::shared_future<void> start(startPromise.get_future());
            std::promise<void> stopPromise;
            std::shared_future<void> stop(stopPromise.get_future());
            int numThreads = 20;
            std::vector<std::promise<void>> readies(numThreads);
            std::vector<std::future<void>> asyncworkservice_futures(numThreads);
            try
            {
                for (int i = 0; i < numThreads; i++)
                {
                    asyncworkservice_futures[i] = std::async(std::launch::async,
                                                             [&scrAsyncWorkService, i, start, stop, &readies]()
                                                             {
                                                                 readies[i].set_value();
                                                                 start.wait();
                                                                 do
                                                                 {
                                                                     std::packaged_task<void()> task(
                                                                         []()
                                                                         {
                                                                             int v = 1 + 2;
                                                                             US_UNUSED(v);
                                                                         });
                                                                     scrAsyncWorkService.post(std::move(task));
                                                                 } while (stop.wait_for(std::chrono::milliseconds(1))
                                                                          != std::future_status::ready);
                                                             });
                }
                for (int i = 0; i < numThreads; i++)
                {
                    readies[i].get_future().wait();
                }
                startPromise.set_value();

                auto serviceReg = std::async(
                    std::launch::async,
                    [start, stop, &bundleContext]()
                    {
                        start.wait();
                        auto mockAsyncWorkService = std::make_shared<cppmicroservices::async::MockAsyncWorkService>();
                        EXPECT_CALL(*mockAsyncWorkService, post(::testing::_)).Times(::testing::AtLeast(1));
                        do
                        {
                            auto reg1 = bundleContext.RegisterService<cppmicroservices::async::AsyncWorkService>(
                                mockAsyncWorkService);
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            reg1.Unregister();
                        } while (stop.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready);
                    });
                std::this_thread::sleep_for(std::chrono::seconds(30));
                stopPromise.set_value();
                serviceReg.get();
                for (int i = 0; i < numThreads; i++)
                {
                    asyncworkservice_futures[i].get();
                }
            }
            catch (...)
            {
                startPromise.set_value();
                stopPromise.set_value();
                throw;
            }
        });
    }

    INSTANTIATE_TEST_SUITE_P(AsyncWorkServiceEndToEndParameterized,
                             TestAsyncWorkServiceEndToEnd,
                             testing::Values(std::make_shared<AsyncWorkServiceThreadPool>(1),
                                             std::make_shared<AsyncWorkServiceThreadPool>(8),
                                             std::make_shared<AsyncWorkServiceThreadPool>(20)));

    TEST_P(TestAsyncWorkServiceEndToEnd, TestEndToEndBehaviorWithAsyncWorkService)
    {
        std::vector<std::string> bundlesToInstall
            = { "DSGraph01", "DSGraph02", "DSGraph03", "DSGraph04", "DSGraph05", "DSGraph06", "DSGraph07" };
        std::vector<cppmicroservices::Bundle> installedBundles;

        EXPECT_NO_THROW({
            auto const& param = GetParam();

            auto ctx = framework.GetBundleContext();

            auto reg = ctx.RegisterService<cppmicroservices::async::AsyncWorkService>(param);

            for (const auto& bundleName : bundlesToInstall)
            {
                installedBundles.emplace_back(::test::InstallAndStartBundle(ctx, bundleName));
            }

            for (auto& bundle : installedBundles)
            {
                bundle.Stop();
            }
        });
    }

    TEST_P(TestAsyncWorkServiceEndToEnd, testAsyncWorkServiceDeadlock)
    {
        auto param = GetParam();

        auto ctx = framework.GetBundleContext();

        // ASYNCWORKSERVICE
        auto reg = ctx.RegisterService<cppmicroservices::async::AsyncWorkService>(param);

        // CA, DS
        ::test::InstallAndStartConfigAdmin(ctx);

        // CA SERVICE
        auto sr = ctx.GetServiceReference<cppmicroservices::service::cm::ConfigurationAdmin>();
        auto configAdmin = ctx.GetService<cppmicroservices::service::cm::ConfigurationAdmin>(sr);

        // CONFIG NAME
        std::string componentName = "sample::ServiceComponentCA06";

        auto bundle = ::test::InstallAndStartBundle(ctx, "TestBundleDSCA06");
        ASSERT_TRUE(bundle);

        // Create configuration object and update property.
        auto configuration = configAdmin->GetConfiguration(componentName);
        auto configInstance = configuration->GetPid();

        cppmicroservices::AnyMap props({
            {"uniqueProp", std::string("instance1")}
        });

        auto fut = configuration->Update(props);
        fut.get();

        auto instanceRef = ctx.GetServiceReference<::test::TestManagedServiceInterface>();
        auto service = ctx.GetService<::test::TestManagedServiceInterface>(instanceRef);

        auto pathI = ::test::GetPathInfo();
        props["libPath"] = pathI["libPath"];
        props["dirSep"] = pathI["dirSep"];
        props["usLibPrefix"] = pathI["usLibPrefix"];
        props["usLibPostfix"] = pathI["usLibPostfix"];
        props["usLibExt"] = pathI["usLibExt"];
        props["context"] = std::make_shared<cppmicroservices::BundleContext>(ctx);

        fut = configuration->Update(props);
        fut.get();

        ASSERT_TRUE(service) << "GetService failed for CAInterface.";
    }

    class postWithSynchronization : public cppmicroservices::async::AsyncWorkService
    {
      public:
        postWithSynchronization() = default;
        virtual ~postWithSynchronization() = default;
        virtual void postWithSync(std::packaged_task<void()>&&, std::shared_ptr<Barrier>) {};
    };

    class AsyncWorkServiceThreadPoolWithSync : public postWithSynchronization
    {
      public:
        AsyncWorkServiceThreadPoolWithSync(int nThreads) : postWithSynchronization()
        {
            threadpool = std::make_shared<boost::asio::thread_pool>(nThreads);
        }

        ~AsyncWorkServiceThreadPoolWithSync() override
        {
            try
            {
                if (threadpool)
                {
                    try
                    {
                        threadpool->stop();
                    }
                    catch (...)
                    {
                        //
                    }
                }
            }
            catch (...)
            {
                //
            }
        }

        void
        post(std::packaged_task<void()>&& task) override
        {
            using Sig = void();
            using Result = boost::asio::async_result<decltype(task), Sig>;
            using Handler = typename Result::completion_handler_type;

            Handler handler(std::forward<decltype(task)>(task));
            Result result(handler);

            boost::asio::post(threadpool->get_executor(), [handler = std::move(handler)]() mutable { handler(); });
        }

        void
        postWithSync(std::packaged_task<void()>&& task, std::shared_ptr<Barrier> barrier) override
        {
            using Sig = void();
            using Result = boost::asio::async_result<decltype(task), Sig>;
            using Handler = typename Result::completion_handler_type;

            Handler handler(std::forward<decltype(task)>(task));
            Result result(handler);

            barrier->Wait();

            boost::asio::post(threadpool->get_executor(), [handler = std::move(handler)]() mutable { handler(); });
        }

        void
        waitForAllPostedTasksToRun() override
        {
            if (threadpool)
            {
                try
                {
                    threadpool->join();
                }
                catch (...)
                {
                    //
                }
            }
        }

      private:
        std::shared_ptr<boost::asio::thread_pool> threadpool;
    };

    // verify that all tasks that are posted do get run even if the service is unregistered mid execution
    TEST_F(TestAsyncWorkServiceEndToEnd, TestTasksRunningAtShutdownSafety)
    {
        using namespace std::literals::chrono_literals;

        auto ctx = framework.GetBundleContext();
        cppmicroservices::ServiceRegistration<postWithSynchronization> reg;
        constexpr int total_tasks = 6;

        {
            // thread pool size should be the total tasks +1 so that we make sure that we don't deadlock because all 6
            // tasks are allocated and waiting on tasks behind them in the queue. However, it has to be small enough
            // that not all the 'external' tasks and 'internal' tasks can be allocated at the same time
            auto param = std::make_shared<AsyncWorkServiceThreadPoolWithSync>(total_tasks + 1);
            reg = ctx.RegisterService<postWithSynchronization, cppmicroservices::async::AsyncWorkService>(param);
        }

        // ASYNCWORKSERVICE
        auto srAWS = ctx.GetServiceReference<postWithSynchronization>();
        auto asyncWorkService = ctx.GetService<postWithSynchronization>(srAWS);
        // CA
        ::test::InstallAndStartConfigAdmin(ctx);

        std::vector<std::shared_future<void>> futs;

        // wait for all post_task objects to be started before second post is made.
        // the '+1' here is to wait till right before the unregister call is made.
        // This tries to ensure that some of these internal post_tasks will be posted but not allocated to a thread
        auto waitForInitialWave = std::make_shared<Barrier>(total_tasks + 1);
        std::atomic<int> counter(total_tasks);

        for (int i = 0; i < total_tasks; ++i)
        {
            std::packaged_task<void()> post_task(
                [asyncWorkService, &waitForInitialWave, &counter]() mutable
                {
                    std::packaged_task<void()> post_task_internal(
                        [&counter]() mutable
                        {
                            // give some time for the unregister call to be successful
                            std::this_thread::sleep_for(500ms);
                            --counter;
                        });

                    auto myFut = post_task_internal.get_future().share();

                    // post with blocker to wait for all post_tasks to make it
                    asyncWorkService->postWithSync(std::move(post_task_internal), waitForInitialWave);
                    // don't store a shared_ptr to the asyncWorkService or it'll never be destroyed
                    asyncWorkService.reset();
                    myFut.get();
                });

            futs.emplace_back(post_task.get_future().share());

            asyncWorkService->post(std::move(post_task));
        }
        asyncWorkService.reset();

        waitForInitialWave->Wait();
        reg.Unregister();

        for (auto& fut : futs)
        {
            fut.get();
        }

        ASSERT_EQ(counter, 0);
    }
}; // namespace test
