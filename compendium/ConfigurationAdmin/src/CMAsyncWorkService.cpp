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

#include "CMAsyncWorkService.hpp"

#include "boost/asio/async_result.hpp"
#include "boost/asio/packaged_task.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"

namespace cppmicroservices
{
    namespace cmimpl
    {

        /**
         * FallbackAsyncWorkService represents the fallback strategy in the event
         * that a AsyncWorkService is not present within the framework. It implements
         * the public interface for AsyncWorkService and is created in the event that
         * a user-provided service was not given or if the user-provided service
         * which implements the AsyncWorkService interface was unregistered.
         */
        class FallbackAsyncWorkService final : public cppmicroservices::async::AsyncWorkService
        {
          public:
            FallbackAsyncWorkService(std::shared_ptr<cppmicroservices::logservice::LogService> const& logger_)
                : logger(logger_)
            {
                Initialize();
            }

            void
            Initialize()
            {
                threadpool = std::make_shared<boost::asio::thread_pool>(1);
            }

            void
            Shutdown()
            {
                if (threadpool)
                {
                    try
                    {
                        threadpool->join();
                        threadpool->stop();
                        threadpool.reset();
                    }
                    catch (...)
                    {
                        auto exceptionPtr = std::current_exception();
                        std::string msg = "An exception has occurred while trying to shutdown "
                                          "the fallback cppmicroservices::async::AsyncWorkService "
                                          "instance.";
                        logger->Log(cppmicroservices::logservice::SeverityLevel::LOG_WARNING, msg, exceptionPtr);
                    }
                }
            }

            ~FallbackAsyncWorkService() { Shutdown(); }

            void
            post(std::packaged_task<void()>&& task) override
            {
                if (threadpool)
                {
                    using Sig = void();
                    using Result = boost::asio::async_result<decltype(task), Sig>;
                    using Handler = typename Result::completion_handler_type;

                    Handler handler(std::forward<decltype(task)>(task));

                    boost::asio::post(threadpool->get_executor(),
                                      [handler = std::move(handler)]() mutable { handler(); });
                }
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
                        auto exceptionPtr = std::current_exception();
                        std::string msg = "An exception has occurred while trying to shutdown "
                                          "the fallback cppmicroservices::async::AsyncWorkService "
                                          "instance.";
                        logger->Log(cppmicroservices::logservice::SeverityLevel::LOG_WARNING, msg, exceptionPtr);
                    }
                }
            }

          private:
            std::shared_ptr<boost::asio::thread_pool> threadpool;
            std::shared_ptr<cppmicroservices::logservice::LogService> logger;
        };

        CMAsyncWorkService::CMAsyncWorkService(cppmicroservices::BundleContext context,
                                               std::shared_ptr<cppmicroservices::logservice::LogService> const& logger_)
            : scrContext(context)
            , serviceTracker(
                  std::make_unique<cppmicroservices::ServiceTracker<cppmicroservices::async::AsyncWorkService>>(context,
                                                                                                                this))
            , asyncWorkService(nullptr)
            , logger(logger_)
        {
            if (auto asyncWSSRef = context.GetServiceReference<cppmicroservices::async::AsyncWorkService>();
                asyncWSSRef)
            {
                asyncWorkService = context.GetService<cppmicroservices::async::AsyncWorkService>(asyncWSSRef);
            }
            else
            {
                asyncWorkService = std::make_shared<FallbackAsyncWorkService>(logger_);
            }

            serviceTracker->Open();
        }

        CMAsyncWorkService::~CMAsyncWorkService() noexcept { asyncWorkService.reset(); }

        void
        CMAsyncWorkService::StopTracking()
        {
            if (serviceTracker)
            {
                serviceTracker->Close();
                serviceTracker.reset();
            }
        }

        std::shared_ptr<cppmicroservices::async::AsyncWorkService>
        CMAsyncWorkService::AddingService(ServiceReference<cppmicroservices::async::AsyncWorkService> const& reference)
        {
            auto currAsync = std::atomic_load(&asyncWorkService);
            std::shared_ptr<cppmicroservices::async::AsyncWorkService> newService;
            if (reference)
            {
                try
                {
                    newService = scrContext.GetService<cppmicroservices::async::AsyncWorkService>(reference);
                    if (newService)
                    {
                        std::atomic_store(&asyncWorkService, newService);
                    }
                }
                catch (...)
                {
                    auto exceptionPtr = std::current_exception();
                    std::string msg = "An exception was caught while retrieving an instance of "
                                      "cppmicroservices::async::AsyncWorkService. Falling "
                                      "back to the default.";
                    logger->Log(cppmicroservices::logservice::SeverityLevel::LOG_WARNING, msg, exceptionPtr);
                }
            }
            return newService;
        }

        void
        CMAsyncWorkService::ModifiedService(
            ServiceReference<cppmicroservices::async::AsyncWorkService> const& /* reference */,
            std::shared_ptr<cppmicroservices::async::AsyncWorkService> const& /* service */)
        {
            // no-op
        }

        void
        CMAsyncWorkService::RemovedService(
            ServiceReference<cppmicroservices::async::AsyncWorkService> const& /* reference */,
            std::shared_ptr<cppmicroservices::async::AsyncWorkService> const& service)
        {
            auto currAsync = std::atomic_load(&asyncWorkService);
            if (service == currAsync)
            {
                auto oldWorkService = currAsync;
                // replace existing asyncWorkService with a fallback asyncWorkService
                std::shared_ptr<cppmicroservices::async::AsyncWorkService> newService
                    = std::make_shared<FallbackAsyncWorkService>(logger);
                std::atomic_store(&asyncWorkService, newService);

                // wait for all running tasks to finish
                oldWorkService->waitForAllPostedTasksToRun();
            }
        }

        void
        CMAsyncWorkService::post(std::packaged_task<void()>&& task)
        {
            auto currAsync = std::atomic_load(&asyncWorkService);
            currAsync->post(std::move(task));
        }

    } // namespace cmimpl
} // namespace cppmicroservices