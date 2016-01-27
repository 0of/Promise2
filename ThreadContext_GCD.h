/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef THREAD_CONTEXT_STL_H
#define THREAD_CONTEXT_STL_H

typedef struct dispatch_queue_s *dispatch_queue_t; 

#include "PromisePublicAPIs"

namespace ThreadContextImpl {
  namespace GCD {
    class QueueBasedThreadContext : public ThreadContext {
    public:
      static ThreadContext *New(dispatch_queue_t queue);

    private:
      dispatch_queue_t _queue;

    protected:
      QueueBasedThreadContext() = default;

    public:
      virtual ~QueueBasedThreadContext() = default;

    public:
      virtual void scheduleToRun(std::function<void()>&& task) override;

    private:
      QueueBasedThreadContext(const QueueBasedThreadContext& ) = delete;
      QueueBasedThreadContext& operator = (const QueueBasedThreadContext& ) = delete;
    };

    class CurrentThreadContext : public ThreadContext {
    public:
      static ThreadContext *New();
    };

    class MainThreadContext : public ThreadContext {
    public:
      static ThreadContext *New();
    };
  } // GCD
} 

#endif // THREAD_CONTEXT_STL_H
