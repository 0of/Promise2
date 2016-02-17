/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef THREAD_CONTEXT_STL_H
#define THREAD_CONTEXT_STL_H

#include "PromiseConfig.h"

#if USE_DISPATCH

#include <dispatch/dispatch.h>

#include "PromisePublicAPIs"

typedef struct dispatch_queue_s *dispatch_queue_t; 

namespace ThreadContextImpl {
  namespace GCD {
    namespace Details {
      using Task = std::function<void()>;

      static void InvokeFunction(void *context) {
        std::unique_ptr<Task> function{ std::static_cast<Task *>(context) };
        (*function)();
      }
    }

    class QueueBasedThreadContext : public ThreadContext {
    public:
      static ThreadContext *New(dispatch_queue_t queue) {
        QueueBasedThreadContext *context = new QueueBasedThreadContext;
        if (context) {
          context->_queue = queue;
        }
        
        return context;
      }

    private:
      dispatch_queue_t _queue;

    protected:
      QueueBasedThreadContext() = default;

    public:
      virtual ~QueueBasedThreadContext() = default;

    public:
      virtual void scheduleToRun(std::function<void()>&& task) override {
        dispatch_async_f(_queue, new Details::Task{ std::move(task) }, Details::InvokeFunction);
      }

    private:
      QueueBasedThreadContext(const QueueBasedThreadContext& ) = delete;
      QueueBasedThreadContext& operator = (const QueueBasedThreadContext& ) = delete;
    };

    class CurrentThreadContext : public ThreadContext {
    public:
      static ThreadContext *New() {
        return QueueBasedThreadContext::New(dispatch_get_current_queue());
      }
    };

    class MainThreadContext : public ThreadContext {
    public:
      static ThreadContext *New() {
        return QueueBasedThreadContext::New(dispatch_get_main_queue());
      }
    };
  } // GCD
}

#endif // USE_DISPATCH

#endif // THREAD_CONTEXT_STL_H
