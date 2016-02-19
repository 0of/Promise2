/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef THREAD_CONTEXT_WINDOWS_H
#define THREAD_CONTEXT_WINDOWS_H

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>

#include "PromisePublicAPIs.h"

namespace ThreadContextImpl {
  namespace Windows {
    namespace Details {
      using Task = std::function<void()>;

      static void InvokeFunction(void *context) {
        std::unique_ptr<Task> function{ static_cast<Task *>(context) };
        (*function)();
      }
    } // Details

    class ThreadPoolContext : public Promise2::ThreadContext {
    public:
      static ThreadContext *New() {
        return new ThreadPoolContext;
      }

    protected:
      ThreadPoolContext() = default;

    public:
      virtual ~ThreadPoolContext() = default;

    public:
      virtual void scheduleToRun(std::function<void()>&& task) override {
        ::QueueUserWorkItem(Details::InvokeFunction, new Details::Task{ std::move(task) }, 0);
      }

    private:
      ThreadPoolContext(const ThreadPoolContext& ) = delete;
      ThreadPoolContext& operator = (const ThreadPoolContext& ) = delete;
    };
  } // Windows
} 

#endif // windows

#endif // THREAD_CONTEXT_WINDOWS_H
