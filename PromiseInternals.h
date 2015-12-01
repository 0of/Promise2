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
#include <atomic>

namespace Promise2 {
  // 
  // @class ThreadContext
  //
  class ThreadContext {
  public:
    virtual ~ThreadContext() = default;

  public:
    virtual void scheduleToRun(std::function<void()>&& task) = 0;
  };
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

    public:
      virtual void chainNext(const std::shared_ptr<PromiseNode>& ) = 0;

      virtual std::function<void()> acquireNotify() = 0;
    };

    //
    // fulfill traits
    //
    template<typename FulfillArgType>
    class Fulfill {
    private:
      std::future<FulfillArgType> _previousPromise;

      std::atomic_flag _attachGuard;
      std::atomic_bool _hasAttached;

    protected:
      Fulfill()
        : _previousPromise{}
        , _attachGuard{ ATOMIC_FLAG_INIT }
        , _hasAttached{ false }
      {}

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
        if (_attachGuard.test_and_set()) {
          // already attached or is attaching
          throw std::exception("");
        }

        _previousPromise = std::move(previousPromise);
        // attached
        _hasAttached = true;
      }

      bool isAttached() const {
        return _hasAttached;
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

      std::atomic_flag _chainingGuard;
      std::atomic_bool _hasChained;
      std::atomic_bool _hasFinished;

    public:
      Forward()
        : _chainingGuard{ ATOMIC_FLAG_INIT }
        , _hasChained{ false }
        , _hasFinished{ false } {
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
      void doChaining(Fulfill<ForwardType>& fulfill, std::function<void()>&& notify) {
        if (_chainingGuard.test_and_set()) {
          // already chained or is chaining
          throw std::exception("");
        }

        try {

          fulfill.attach(std::move(_forwardPromise.get_future()));

          // update notifier
          _notify = std::move(notify);

        } catch (const std::exception& e) {
          // something goes wrong
          // clean the guard flag
          _chainingGuard.clear();
          throw e;
        }
        
        _hasChained = true;

        // if already finished notify the next node now
        if (_hasFinished) {
          _notify();
        }
      }

      template<typename T>
      void fulfill(T&& value) {
        _forwardPromise.set_value(std::forward<T>(value));
        _hasFinished = true;
      }

      void reject(std::exception_ptr exception) {
        _forwardPromise.set_exception(exception);
        _hasFinished = true;
      }

      bool hasChained() const {
        return _hasChained;
      }
    };

    //
    // Promise node
    //
	template<typename ReturnType, typename ArgType, 
           typename ForwardTrait = Forward<ReturnType>>
    class PromiseNodeInternal : public PromiseNode
                              , public Fulfill<ArgType>
                              , public std::enable_shared_from_this<PromiseNodeInternal<ReturnType, ArgType, ForwardTrait>>{
    private:
      ForwardTrait _forward;
      
      std::shared_ptr<ThreadContext> _context;

      std::function<ReturnType(ArgType)> _onFulfill;
      std::function<void(std::exception_ptr)> _onReject;

      std::once_flag _called;

    public:
      PromiseNode(std::function<ReturnType(ArgType)>&& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  std::shared_ptr<ThreadContext>&& context)
        : PromiseNode()
        , Fulfill<ArgType>()
        , _context{ std::move(context) }
      {}

    public:
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

      virtual std::function<void()> acquireNotify() override {
        auto&& run = std::bind(&run, shared_from_this());
        return std::move(std::bind(&ThreadContext::scheduleToRun, _context, std::move(run)));
      }

    public:
      virtual void chainNext(const std::shared_ptr<PromiseNode>& node) override {
        if (node == this) {
          throw std::exception("");
        }

        // check self has been chained a node
        if (_forward.hasChained()) {
          throw std::exception("");
        }

        auto fulfill = std::static_pointer_cast<Fulfill<ArgType>>(node);

        if (fulfill->isAttached()) {
          throw std::exception("");
        }

        _forward.doChaining(fulfill, node->acquireNotify());
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
      PromiseNode(PromiseNode&& node) = delete;
      PromiseNode(const PromiseNode&) = delete;
    };
  }
}

#endif