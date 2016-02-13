/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef THREAD_CONTEXT_STL_H
#define THREAD_CONTEXT_STL_H

#include <thread>

#include "PromisePublicAPIs.h"

namespace ThreadContextImpl {
  namespace STL {
    class DetachedThreadContext : public Promise2::ThreadContext {
    public:
      static Promise2::ThreadContext *New() {
        return new DetachedThreadContext;
      }

    private:
      std::thread _detachedThread;

    protected:
      DetachedThreadContext() = default;

    public:
      virtual ~DetachedThreadContext() = default;

    public:
      virtual void scheduleToRun(std::function<void()>&& task) override {
        std::thread taskThread{ std::move(task) };
        _detachedThread = std::move(taskThread);

        _detachedThread.detach();
      }

    private:
      DetachedThreadContext(const DetachedThreadContext& ) = delete;
      DetachedThreadContext& operator = (const DetachedThreadContext& ) = delete;
    };
  } // STL
} 

#endif // THREAD_CONTEXT_STL_H
