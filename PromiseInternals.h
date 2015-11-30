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
      virtual void chainNext(const std::shared_ptr<PromiseNode>& ) = 0;

      virtual ThreadContext context() const = 0;
    };

    //
    // fulfill traits
    //
    template<typename FulfillArgType>
    class Fulfill {
    private:
      std::future<FulfillArgType> _previousPromise;

    public:
      virtual ~Fulfill() = default;

    protected:
      FulfillArgType get() { 
        // has any previous promise
        if (!_previousPromise.valid()) {
          throw std::exception("");
        }

        return _previousPromise.get();
      }

      void attach(std::future<FulfillArgType>&& previousPromise) {
        _previousPromise = std::move(previousPromise);
      }

      bool isAttached() const {
        return _previousPromise.valid();
      }
    };

    //
    // forward traits
    //
    template<typename ForwardType>
    class Forward {
    private:
      std::promise<ForwardType> _forwardPromise; 
      std::future<ForwardType> _forwardFuture;

      std::function<void()> _notify;

    public:
      Forward() {
        _forwardFuture = std::move(_forwardPromise.get_future());
      }
      ~Forward() = default;

    public:
      void notify() {
        if (_notify) {
          _notify();
        }
      }

    public:
      void doChaining(Fulfill<ForwardType>& fulfill, const ThreadContext& context) {
        fulfill.attach(std::move(_forwardPromise.get_future()));
        // update notify
        // TODO:
      }

      template<typename T>
      void fulfill(T&& value) {
        _forwardPromise.set_value(std::forward<T>(value));
      }

      void reject(std::exception_ptr exception) {
        _forwardPromise.set_exception(exception);
      }

      bool hasMovedFuture() const {
        return !_forwardFuture.valid();
      }
    };

    //
    // Promise node
    //
	template<typename ReturnType, typename ArgType, 
           typename ForwardTrait = Forward<ReturnType>>
    class PromiseNodeInternal : public PromiseNode, public Fulfill<ArgType> {
    private:
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
      virtual void postToRun() {
        // TODO:
      }

      virtual void run() override {
            // ensure calling once
        std::call_once(_called, [&]() {

          bool safelyDone = false;

          try {
            safelyDone = runFulfill(get());
          } catch (...) {
            // previous task is failed
            runReject();
          }

          // in some case, previous one already failed
          // propage the exception
          if (!safelyDone) {
            // clean the status
            _forward.notify();
          }
        });
      }

    public:
      virtual void chainNext(const std::shared_ptr<PromiseNode>& node) override {
        // check self has been chained a node
        if (_forward.hasMovedFuture()) {
          throw std::exception("");
        }

        if (node == this) {
          throw std::exception("");
        }

        auto&& context = node->context();
        auto fulfill = std::static_pointer_cast<Fulfill<ArgType>>(node);

        if (fulfill->isAttached()) {
          throw std::exception("");
        }

        // chain!
        _forward.doChaining(fulfill, context);
      }

      virtual ThreadContext context() const override {
        return _context;
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
          runReject();
        }

        // I'm done!
        _forward.notify();
        return true;
      }

      // called when exception has been thrown
      void runReject() noexcept {
        if (_onReject) {
          try {
            _onReject(std::current_exception());
          } catch (...) {
            _forward.reject(std::current_exception());
          }
        } else {
          _forward.reject(std::current_exception());
        }
      }

    private:
      PromiseNode(const PromiseNode&) = delete;
    };
  }
}

#endif