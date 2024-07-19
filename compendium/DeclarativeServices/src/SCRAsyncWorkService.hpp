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

#ifndef SCRASYNCWORKSERVICE_HPP
#define SCRASYNCWORKSERVICE_HPP

#include "SCRLogger.hpp"
#include <cppmicroservices/ServiceTracker.h>
#include <cppmicroservices/asyncworkservice/AsyncWorkService.hpp>

#include <future>

namespace cppmicroservices
{
    namespace scrimpl
    {

        class SCRAsyncWorkServiceDetail;

        /**
         * This class is used to track the availability of AsyncWorkService in the
         * framework. If a AsyncWorkService is available, the calls to "post" are forwarded
         * to the AsyncWorkService. Otherwise, the calls to "post" use the pre-defined,
         * default behavior for post. This class implements the AsyncWorkService interface
         * so that other classes within the runtime can easily use a mock async work service for
         * testing purposes.
         */
        class SCRAsyncWorkService final
            : public cppmicroservices::async::AsyncWorkService
            , public cppmicroservices::ServiceTrackerCustomizer<cppmicroservices::async::AsyncWorkService>
        {
          public:
            explicit SCRAsyncWorkService(cppmicroservices::BundleContext context,
                                         std::shared_ptr<cppmicroservices::logservice::LogService> const& logger_);
            SCRAsyncWorkService(SCRAsyncWorkService const&) noexcept = delete;
            SCRAsyncWorkService(SCRAsyncWorkService&&) noexcept = delete;
            SCRAsyncWorkService& operator=(SCRAsyncWorkService const&) noexcept = delete;
            SCRAsyncWorkService& operator=(SCRAsyncWorkService&&) noexcept = delete;
            ~SCRAsyncWorkService() noexcept override;

            // methods from the cppmicroservices::async::AsyncWorkService interface
            void post(std::packaged_task<void()>&& task) override;
            void
            waitForAllPostedTasksToRun() override
            {
                // noop
            }

            // methods from the cppmicroservices::ServiceTrackerCustomizer interface
            std::shared_ptr<TrackedParamType> AddingService(
                ServiceReference<cppmicroservices::async::AsyncWorkService> const& reference) override;
            void ModifiedService(ServiceReference<cppmicroservices::async::AsyncWorkService> const& reference,
                                 std::shared_ptr<cppmicroservices::async::AsyncWorkService> const& service) override;
            void RemovedService(ServiceReference<cppmicroservices::async::AsyncWorkService> const& reference,
                                std::shared_ptr<cppmicroservices::async::AsyncWorkService> const& service) override;

            // method to stop tracking the AsyncWorkService. This must be called from the SCR
            // BundleActivate's Stop method. Not thread-safe. Must not be called simultaneously from
            // multiple threads
            void StopTracking();

          private:
            cppmicroservices::BundleContext scrContext;
            std::unique_ptr<cppmicroservices::ServiceTracker<cppmicroservices::async::AsyncWorkService>> serviceTracker;
            std::shared_ptr<cppmicroservices::async::AsyncWorkService> asyncWorkService;
            std::shared_ptr<cppmicroservices::logservice::LogService> logger;
        };
    } // namespace scrimpl
} // namespace cppmicroservices

#endif
