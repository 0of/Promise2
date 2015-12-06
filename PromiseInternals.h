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
    template<typename FulfillArgType>
    class Fulfill;

  	//
  	// each `Promise` will hold one shared `PromiseNode`
  	//
    template<typename T>
    class PromiseNode {
    public:
      virtual ~PromiseNode() = default;

    public:
      // run the current task under current context
      virtual void run() = 0;

    public:
      virtual void chainNext(const std::shared_ptr<Fulfill<T>>&, std::function<void()>&& notify) = 0;
    };

    class PromiseValueBase {
    protected:
      std::exception_ptr _exception;

      std::atomic_flag _assignGuard;
      std::atomic_bool _hasAssigned;

    protected:
      PromiseValueBase()
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
      PromiseValueBase(const PromiseValueBase&) = delete;
      PromiseValueBase(const PromiseValueBase&&) = delete;
    };


    template<typename T>
    class PromiseValue : public PromiseValueBase {
    public:
      // allow to direct access
      T value;

    public:
      PromiseValue()
        : value()
      {}

      ~PromiseValue() = default;

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
    class PromiseValue<void> : public PromiseValueBase {
    public:
      PromiseValue() = default;
      ~PromiseValue() = default;

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
    template<typename T> using SharedPromiseValue = std::shared_ptr<PromiseValue<T>>;


    template<typename FulfillArgType>
    class FulfillGet {
    protected:
      SharedPromiseValue<FulfillArgType> _previousPromise;

    public:
      bool isAttached() const {
        return !!_previousPromise;
      }

    protected:
      FulfillArgType get() {
        if (!isAttached()) {
          throw std::logic_error("invalid promise state");
        }

        _previousPromise->accessGuard();
        return _previousPromise->value;
      }
    };

    template<>
    class FulfillGet<void> {
    protected:
      SharedPromiseValue<void> _previousPromise;

    public:
      bool isAttached() const {
        return !!_previousPromise;
      }

    protected:
      void get() {
        if (!isAttached()) {
          throw std::logic_error("invalid promise state");
        }

        _previousPromise->accessGuard();
      }
    };

    //
    // fulfill traits
    //
    template<typename FulfillArgType>
    class Fulfill : public FulfillGet<FulfillArgType> {
    private:
      std::atomic_flag _attachGuard;

    protected:
      Fulfill()
        : FulfillGet<FulfillArgType>()
        , _attachGuard{ ATOMIC_FLAG_INIT }
      {}

    public:
      virtual ~Fulfill() = default;

    protected:
      void attach(const SharedPromiseValue<FulfillArgType>& previousPromise) {
        if (_attachGuard.test_and_set()) {
          // already attached or is attaching
          throw std::logic_error("promise duplicated attachments");
        }

        FulfillGet<FulfillArgType>::_previousPromise = previousPromise;
      }
    };

    //
    // forward traits
    //
    template<typename ForwardType>
    class Forward {
    private:
      SharedPromiseValue<ForwardType> _promise;

      std::function<void()> _notify;

      std::atomic_flag _chainingGuard;
      std::atomic_bool _hasChained;

    public:
      Forward()
        : _promise{ }
        , _chainingGuard{ ATOMIC_FLAG_INIT }
        , _hasChained{ false } 
      {}
      ~Forward() = default;

    public:
      void doChaining(const std::shared_ptr<Fulfill<ForwardType>>& fulfill, std::function<void()>&& notify) {
        if (_chainingGuard.test_and_set()) {
          // already chained or is chaining
          throw std::logic_error("promise duplicated chainings");
        }

        try {

          fulfill->attach(_promise);

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
        if (_notify) {
          _notify();
        }
      }

      void reject(std::exception_ptr exception) {
        _promise.setException(exception);
        if (_notify) {
          _notify();
        }
      }

      bool hasChained() const {
        return _hasChained;
      }
    };

    template<typename ReturnType, typename ArgType>
    class PromiseNodeInternal;

    //
    //  @alias
    //
    template<typename ReturnType, typename ArgType>
    using EnableShared = std::enable_shared_from_this<PromiseNodeInternal<ReturnType, ArgType>>;

    template<typename T> using DeferPromiseCore = std::unique_ptr<Forward<T>>;

    template<typename ReturnType, typename ArgType>
    class PromiseNodeInternalBase : public PromiseNode<ReturnType>
                              , public Fulfill<ArgType> {
    protected:
      DeferPromiseCore<ReturnType> _forward;
      std::shared_ptr<ThreadContext> _context;

      std::function<void(std::exception_ptr)> _onReject;
      std::once_flag _called;

    public:
      PromiseNodeInternalBase(std::function<void(std::exception_ptr)>&& onReject,
                  std::shared_ptr<ThreadContext>&& context)
        : PromiseNode<ReturnType>()
        , Fulfill<ArgType>()
        , _forward{ std::make_unique<DeferPromiseCore<ReturnType>::element_type>() }
        , _onReject{ std::move(onReject) }
        , _context{ std::move(context) }
      {}

    public:
      virtual void chainNext(const std::shared_ptr<Fulfill<ReturnType>>& fulfill, std::function<void()>&& notify) override {
        if (fulfill == this) {
          throw std::logic_error("invalid chaining state");
        }

        _forward->doChaining(fulfill, std::move(notify));
      }

    protected:
      // called when exception has been thrown
      void runReject() noexcept {
        if (_onReject) {
          try {
            _onReject(std::current_exception());
          } catch (...) {
            _forward->reject(std::current_exception());
          }
        } else {
          _forward->reject(std::current_exception());
        }
      }

    private:
      PromiseNodeInternalBase(PromiseNodeInternalBase&& node) = delete;
      PromiseNodeInternalBase(const PromiseNodeInternalBase&) = delete;
    };

    //
    // Promise node
    //
	  template<typename ReturnType, typename ArgType>
    class PromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType>;

    private:
      std::function<ReturnType(ArgType)> _onFulfill;

    public:
      PromiseNodeInternal(std::function<ReturnType(ArgType)>&& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  std::shared_ptr<ThreadContext>&& context)
        : PromiseNodeInternalBase<ReturnType, ArgType>{ std::move(onReject), std::move(context) }
        , _onFulfill{ std::move(onFulfill) }
      {}

    public:
      virtual void run() override {
        std::call_once(Base::_called, [&]() {
          ArgType preValue;

          try {
            preValue = Fulfill<ArgType>::get();
          } catch (...) {
            Base::runReject();
            return;
          }

          try {
            runFulfill(preValue);
          } catch (...) {
            // previous task is failed
            Base::runReject();
          }
        });
      }

    protected:
      template<typename T>
      void runFulfill(T&& preFulfilled) noexcept {
        // warpped the exception
        try {
          Base::_forward->fulfill(
            _onFulfill(std::forward<T>(preFulfilled))
          );
        } catch (...) {
          Base::runReject();
        }
      }
    }; 
  // end of details
  }
}

namespace Promise2 {
  //
  // @class PromiseDefer
  //
  template<typename T>
  class PromiseDefer {
  private:
    Details::DeferPromiseCore<T> _core;

  public:
    PromiseDefer(Details::DeferPromiseCore<T>&& core)
      : _core{ std::move(core) }
    {}

    PromiseDefer(PromiseDefer&&) = default;
    ~PromiseDefer() = default;

  public:
    template<T>
    void setResult(T&& r) {
      _core->setValue(std::forward<T>(r));
    }

    void setException(std::exception_ptr e) {
      _core->setException(e);
    }

  private:
    PromiseDefer(const PromiseDefer&) = delete;
    PromiseDefer& operator = (const PromiseDefer&) = delete;
  };
}

namespace Promise2 {
  namespace Details {
    // deferred PromiseNodeInternal
    template<typename ReturnType, typename ArgType>
    class DeferredPromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType>;

    private:
      std::function<void(PromiseDefer<ReturnType>&&, ArgType)> _onFulfill;

    public:
      DeferredPromiseNodeInternal(std::function<void(PromiseDefer<ReturnType>&&, ArgType)>& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  std::shared_ptr<ThreadContext>&& context)
        : PromiseNodeInternalBase<ReturnType, ArgType>{ std::move(onReject), std::move(context) }
        , _onFulfill{ std::move(onFulfill) }
      {}

    public:
      virtual void run() override {
        std::call_once(Base::_called, [&]() {
          ArgType preValue;

          try {
            preValue = Fulfill<ArgType>::get();
          } catch (...) {
            Base::runReject();
            return;
          }

          try {
            PromiseDefer<ReturnType> deferred{ std::move(Base::_forward) };
            _onFulfill(deferred, preValue);
          } catch (...) {
            // previous task is failed
            Base::runReject();
          }
        });
      }
    };
  } // Details
}

#endif