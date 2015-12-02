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
#include <atomic>
#include <exception>

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

    class SharedPromiseEBC {
    protected:
      std::exception_ptr _exception;

      std::atomic_flag _assignGuard;
      std::atomic_bool _hasAssigned;

    protected:
      SharedPromiseEBC()
        : _exception{ nullptr }
        , _assignGuard{ ATOMIC_FLAG_INIT }
        , _hasAssigned{ false }
      {}

    public:
      void setException(std::exception_ptr e) {
        if (_assignGuard.test_and_set()) {
          // already assigned or is assigning
          throw std::logic_error("promise duplicated assignments");
        }

        _exception = e;
        _hasAssigned = true;
      }

    public:
      //
      // access guard must be invoked before directly access the `value` 
      // check the assigned flag and make sure no exception has been thrown
      //
      void accessGuard() {
        if (!_hasAssigned) {
          throw std::logic_error("promise invalid state");
        }

        if (_exception) {
          std::rethrow_exception(_exception);
        }
      }

    public:
      bool hasAssigned() const {
        return _hasAssigned;
      }

    private:
      SharedPromiseEBC(const SharedPromiseEBC&) = delete;
      SharedPromiseEBC(const SharedPromiseEBC&&) = delete;
    };


    template<typename T>
    class SharedPromise : public SharedPromiseEBC {
    public:
      // allow to direct access
      T value;

    public:
      SharedPromise()
        : value()
      {}

      ~SharedPromise() = default;

    public:
      template<typename ValueType>
      void setValue(ValueType&& v) {
        if (_assignGuard.test_and_set()) {
          // already assigned or is assigning
          throw std::logic_error("promise duplicated assignments");
        }

        value = std::forward<ValueType>(v);
        _hasAssigned = true;
      }
    };

    template<>
    class SharedPromise<void> : public SharedPromiseEBC {
    public:
      SharedPromise() = default;
      ~SharedPromise() = default;

    public:
      void setValue() {
        if (_assignGuard.test_and_set()) {
          // already assigned or is assigning
          throw std::logic_error("promise duplicated assignments");
        }

        _hasAssigned = true;
      }
    };

    //
    //  @alias
    //    
    template<typename T> using DeferPromiseCore = std::shared_ptr<SharedPromise<T>>;


    //
    // fulfill traits
    //
    template<typename FulfillArgType>
    class Fulfill {
    private:
      DeferPromiseCore<FulfillArgType> _previousPromise;
      std::atomic_flag _attachGuard;

    protected:
      Fulfill()
        : _previousPromise()
        , _attachGuard{ ATOMIC_FLAG_INIT }
      {}

    public:
      virtual ~Fulfill() = default;

    protected:
      FulfillArgType get() {
        if (!isAttached()) {
          throw std::logic_error("invalid promise state");
        }

        _previousPromise->accessGuard();
        return _previousPromise->value;
      }

      void attach(const DeferPromiseCore<FulfillArgType>& previousPromise) {
        if (_attachGuard.test_and_set()) {
          // already attached or is attaching
          throw std::logic_error("promise duplicated attachments");
        }

        _previousPromise = previousPromise;
      }

      bool isAttached() const {
        return !!_previousPromise;
      }
    };

    //
    // forward traits
    //
    template<typename ForwardType>
    class Forward {
    private:
      DeferPromiseCore<ForwardType> _promise;

      std::function<void()> _notify;

      std::atomic_flag _chainingGuard;
      std::atomic_bool _hasChained;

    public:
      Forward()
        : _chainingGuard{ ATOMIC_FLAG_INIT }
        , _hasChained{ false } 
      {}
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
          throw std::logic_error("promise duplicated chainings");
        }

        try {

          fulfill.attach(_promise);

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
        if (_promise->hasAssigned()) {
          _notify();
        }
      }

      template<typename T>
      void fulfill(T&& value) {
        _promise.setValue(std::forward<T>(value));
      }

      void reject(std::exception_ptr exception) {
        _promise.setException(exception);
      }

      bool hasChained() const {
        return _hasChained;
      }
    };

    template<typename ReturnType, typename ArgType, typename ForwardTrait>
    class PromiseNodeInternal;

    template<typename ReturnType, typename ArgType, typename ForwardTrait>
    using EnableShared = std::enable_shared_from_this<PromiseNodeInternal<ReturnType, ArgType, ForwardTrait>>;
    //
    // Promise node
    //
	 template<typename ReturnType, typename ArgType, 
           typename ForwardTrait = Forward<ReturnType>>
    class PromiseNodeInternal : public PromiseNode
                              , public Fulfill<ArgType>
                              , EnableShared<ReturnType, ArgType, ForwardTrait> {
    private:
      ForwardTrait _forward;
      
      std::shared_ptr<ThreadContext> _context;

      std::function<ReturnType(ArgType)> _onFulfill;
      std::function<void(std::exception_ptr)> _onReject;

      std::once_flag _called;

    public:
      PromiseNodeInternal(std::function<ReturnType(ArgType)>&& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  std::shared_ptr<ThreadContext>&& context)
        : PromiseNode()
        , Fulfill<ArgType>()
        , _onFulfill{ std::move(onFulfill) }
        , _onReject{ std::move(onReject) }
        , _context{ std::move(context) }
      {}

    public:
      virtual void run() override {
            // ensure calling once
        std::call_once(_called, [&]() {

          bool safelyDone = false;

          try {
            safelyDone = runFulfill(Fulfill<ArgType>::get());
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
        auto&& run = std::bind(&PromiseNode::run, EnableShared<ReturnType, ArgType, ForwardTrait>::shared_from_this());
        return std::move(std::bind(&ThreadContext::scheduleToRun, _context, std::move(run)));
      }

    public:
      virtual void chainNext(const std::shared_ptr<PromiseNode>& node) override {
        if (node == this) {
          throw std::logic_error("invalid chaining state");
        }

        auto fulfill = std::static_pointer_cast<Fulfill<ArgType>>(node);
        _forward.doChaining(fulfill, node->acquireNotify());
      }

    protected:
      template<typename T>
      bool runFulfill(T&& preFulfilled) noexcept {
        // warpped the exception
        try {
          _forward.fulfill(
            _onFulfill(std::forward<T>(preFulfilled))
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
      PromiseNodeInternal(PromiseNodeInternal&& node) = delete;
      PromiseNodeInternal(const PromiseNodeInternal&) = delete;
    };
  }
}

#endif