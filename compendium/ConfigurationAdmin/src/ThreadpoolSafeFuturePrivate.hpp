/*=============================================================================

  Library: CppMicroServices


Copyright Kevlin Henney, 2000, 2001, 2002. All rights reserved.
Extracted from Boost 1.46.1 and adapted for CppMicroServices.

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

=========================================================================*/

#ifndef CPPMICROSERVICES_THREADPOOLSAFEFUTUREPRIVATE_H
#define CPPMICROSERVICES_THREADPOOLSAFEFUTUREPRIVATE_H

#include "cppmicroservices/ThreadpoolSafeFuture.h"

#include <future>

namespace cppmicroservices::cmimpl
{

    using ActualTask = std::packaged_task<void(bool)>;
    using PostTask = std::packaged_task<void()>;
    class ThreadpoolSafeFuturePrivate : public cppmicroservices::ThreadpoolSafeFuture
    {
      public:
        ThreadpoolSafeFuturePrivate(std::shared_future<void> future,
                                    std::shared_ptr<std::atomic<bool>> asyncStarted = nullptr,
                                    std::shared_ptr<ActualTask> task = nullptr);
        ThreadpoolSafeFuturePrivate() = default;

        // Destructor
        ~ThreadpoolSafeFuturePrivate() = default;

        ThreadpoolSafeFuturePrivate(ThreadpoolSafeFuturePrivate const& other) = delete;
        ThreadpoolSafeFuturePrivate& operator=(ThreadpoolSafeFuturePrivate const& other) = delete;
        ThreadpoolSafeFuturePrivate(ThreadpoolSafeFuturePrivate&& other) noexcept = default;
        ThreadpoolSafeFuturePrivate& operator=(ThreadpoolSafeFuturePrivate&& other) noexcept = default;

        // Method to get the result
        void get() const;
        void wait() const;
        template <class Rep, class Period>
        std::future_status
        wait_for(std::chrono::duration<Rep, Period> const& timeout_duration) const
        {
            return future.wait_for(timeout_duration);
        }

        std::shared_future<void>
        retrieveFuture()
        {
            return future;
        }

      private:
        std::shared_future<void> future;
        std::shared_ptr<std::atomic<bool>> asyncStarted;
        std::shared_ptr<ActualTask> task;
    };
} // namespace cppmicroservices::cmimpl

#endif // CPPMICROSERVICES_THREADPOOLSAFEFUTUREPRIVATE_H
