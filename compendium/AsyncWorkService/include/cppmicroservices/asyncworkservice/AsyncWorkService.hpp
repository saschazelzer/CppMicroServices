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
#ifndef CPPMICROSERVICES_ASYNC_WORK_SERVICE_HPP
#define CPPMICROSERVICES_ASYNC_WORK_SERVICE_HPP

#include "cppmicroservices/asyncworkservice/AsyncWorkServiceExport.h"

#include "cppmicroservices/ServiceReferenceBase.h"

#include <future>

namespace cppmicroservices
{
    namespace async
    {

        /**
        \defgroup gr_asyncworkservice AsyncWorkService

        \brief Groups AsyncWorkService class related symbols
        */

        /**
         * \ingroup MicroService
         * \ingroup gr_asyncworkservice
         *
         * Provides a method which controls how DeclarativeServices internally schedules asynchronous work.
         * Creating an AsyncWorkService implementation is not required; this is intended to be used in specialty
         * situations where the client application has requirements that the default asynchronous work scheduling
         * mechanism does not conform to.
         *
         * @remarks This class is thread safe.
         */
        class US_usAsyncWorkService_EXPORT AsyncWorkService
        {
          public:
            virtual ~AsyncWorkService();

            /**
             * Run a std::packaged_task<void()> (optionally on another thread asynchronously).
             * The std::future<void> associated with the std::packaged_task<void()> task
             * object will contain the result from the task object.
             *
             * @param task A std::packaged_task<void()> wrapping a Callable target to
             * execute asynchronously.
             *
             * @note The caller is required to manage the std::future<void> associated
             * with the std::packaged_task<void()> in order to wait on the async task.
             */
            virtual void post(std::packaged_task<void()>&& task) = 0;

            /**
             * Method to wait for all tasks posted to the service to finish
             */
            virtual void
            waitForAllPostedTasksToRun() = 0;
        };
    } // namespace async
} // namespace cppmicroservices

#endif // CPPMICROSERVICES_ASYNC_WORK_SERVICE_HPP
