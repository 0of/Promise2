/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef PROMISE_2_INTERNALS
#define PROMISE_2_INTERNALS

#include <thread>
#include <memory>
#include <functional>
#include <future>
#include <mutex>

/// MARK: stub class
namespace Promise2 {
  class ThreadContext {};
}

namespace Promise2 {
  namespace Details {
  	//
  	// each `Promise` will hold one shared `PromiseNode`
  	//
  	class PromiseNode {
    public:
      virtual ~PromiseNode() = default;

    public:
      // run the current task under current context
      virtual void run() = 0;
      // post to run the task
      virtual void postToRun() = 0;

    public:
      virtual void chainNext() = 0;
    };

    //
    // hold some info of the next chained promise node
    //
  	class PromiseNotify {
      public:
        virtual ~PromiseNotify() = default;

      public:
      	// when current task is done, call this to notify the next one
        virtual void notify() = 0;
    };


    //
    // fulfill traits
    //
    template<typename FulfillArgType>
    class Fulfill {
    private:
      std::future<FulfillArgType> _previousPromise;

    public:
      ~Fulfill() = default;

    public:
      FulfillArgType get() { 
        // has any previous promise
        if (!_previousPromise.valid()) {
          throw std::exception("");
        }

        return _previousPromise.get();
      }

      void attach(std::future<FulfillArgType>&& previousPromise) {
        // already attached
        if (_previousPromise.valid()) {
          throw std::exception("");
        }

        _previousPromise = std::move(previousPromise);
      }
    };

    //
    // forward traits
    //
    template<typename ForwardType>
    class Forward : public PromiseNotify {
    private:
      std::promise<ForwardType> _forwardPromise; 

    public:
      ~Forward() = default;

    public:
      virtual void notify() override {
        // TODO
      }

    public:
      std::future<ForwardType>&& getFuture() {
        return std::move(_forwardPromise.get_future());
      }

      template<typename T>
      void fulfill(T&& value) {
        _forwardPromise.set_value(std::forward<T>(value));
      }

      void reject(std::exception_ptr exception) {
        _forwardPromise.set_exception(exception);
      }
    };

    //
    // Promise node
    //
	template<typename ReturnType, typename ArgType, 
           typename FulfillTrait = Fulfill<ArgType>,
           typename ForwardTrait = Forward<ReturnType>>
    class PromiseNodeInternal : public PromiseNode {
    private:
      FulfillTrait _fulfill;
      ForwardTrait _forward;
      
      // readonly
      const ThreadContext _context;

      std::function<ReturnType(ArgType)> _onFulfill;
      std::function<void(std::exception_ptr)> _onReject;

      std::once_flag _called;

    public:
      PromiseNode(std::function<ReturnType(ArgType)>&& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  const ThreadContext& context);
      PromiseNode(PromiseNode&& node) noexcept;

    public:
      void run() {
            // ensure calling once
        std::call_once(_called, [&]() {

          bool safelyDone = false;

          try {
            safelyDone = runFulfill(_fulfill.get());
          } catch (...) {
            // previous task is failed


            _forward.reject(std::current_exception());
          }

          // in some case, previous one already failed
          // propage the exception
          if (!safelyDone) {
            // clean the status
            _forward.notify();
          }
        });
      }

    protected:
      template<typename T>
      bool runFulfill(T&& preFulfilled) noexcept {
        // warpped the exception
        try {
          _forward.fulfill(
            _onFulfill(std::forward<T>(preFulfilled));
          );
        } catch (...) {
          _forward.reject(std::current_exception());
        }

        // I'm done!
        _forward.notify();
        return true;
      }

    private:
      PromiseNode(const PromiseNode&) = delete;
    };
  }
}

#endif