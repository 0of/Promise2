#ifndef PROMISE_INTERNALS_BASE_H
#define PROMISE_INTERNALS_BASE_H

#include <thread>
#include <memory>
#include <future>
#include <atomic>
#include <exception>
#include <type_traits>

namespace Promise2 {
  namespace Details {
    //
    template<typename FulfillArgType, typename IsTask>
    class Fulfill;
      
    template<typename FulfillArgType>
    using SharedNonTaskFulfill = std::shared_ptr<Fulfill<FulfillArgType, std::false_type>>;

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
      virtual void chainNext(const SharedNonTaskFulfill<T>&, std::function<void()>&& notify) = 0;
      // proxy fowarding
      virtual void chainNext(const DeferPromiseCore<T>&) = 0;

    public:
      virtual bool isFulfilled() const = 0;
      virtual bool isRejected() const = 0;
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
      std::exception_ptr fetchException() const {
        return _exception;
      }

    public:
      bool hasAssigned() const {
        return _hasAssigned;
      }

      bool isExceptionCase() const {
        return nullptr != _exception;
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


    template<typename FulfillArgType, typename IsTask>
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

      void assign(const SharedPromiseValue<FulfillArgType>& v) {
        _previousPromise = v;
      }
    };

    template<typename IsTask>
    class FulfillGet<void, IsTask> {
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

      void assign(const SharedPromiseValue<void>& v) {
        _previousPromise = v;
      }
    };

    template<>
    class FulfillGet<void, std::true_type> {
    public:
      bool isAttached() const {
        return true;
      }

    protected:
      void get() {}
      void assign(const SharedPromiseValue<void>&) {}
    };

    //
    // fulfill traits
    //
    template<typename FulfillArgType, typename IsTask>
    class Fulfill : public FulfillGet<FulfillArgType, IsTask> {
    private:
      std::atomic_flag _attachGuard;

    protected:
      Fulfill()
        : FulfillGet<FulfillArgType, IsTask>()
        , _attachGuard{ ATOMIC_FLAG_INIT }
      {}

    public:
      virtual ~Fulfill() = default;

    public:
      void attach(const SharedPromiseValue<FulfillArgType>& previousPromise) {
        if (_attachGuard.test_and_set()) {
          // already attached or is attaching
          throw std::logic_error("promise duplicated attachments");
        }

        FulfillGet<FulfillArgType, IsTask>::assign(previousPromise);
      }
    };

    template<typename ForwardType>
    class ForwardFulfillPolicy {
    public:
      template<typename T>
      static void wrappedFulfill(SharedPromiseValue<ForwardType>& promise, T&& value) {
        promise->setValue(std::forward<T>(value));
      }

      static void wrappedFulfill(SharedPromiseValue<void>& promise) {}
    };

    template<>
    class ForwardFulfillPolicy<void> {
    public:
      template<typename T>
      static void wrappedFulfill(SharedPromiseValue<void>& promise, T&& value) {}

      static void wrappedFulfill(SharedPromiseValue<void>& promise) {
        promise->setValue();
      }
    };

    template<typename ForwardType>
    struct ProxyForwardPolicy {
      static void forwarding(const DeferPromiseCore<ForwardType>& core, const SharedPromiseValue<ForwardType>& promise);
    };

    template<>
    struct ProxyForwardPolicy<void> {
      static void forwarding(const DeferPromiseCore<void>& core, const SharedPromiseValue<void>& promise);
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
        : _promise{ std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>() }
        , _chainingGuard{ ATOMIC_FLAG_INIT }
        , _hasChained{ false } 
      {}
      ~Forward() = default;

    public:
      void doChaining(const SharedNonTaskFulfill<ForwardType>& fulfill, std::function<void()>&& notify) {
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

      void doChaining(const DeferPromiseCore<ForwardType>& nextForward) {
        if (_chainingGuard.test_and_set()) {
          // already chained or is chaining
          throw std::logic_error("promise duplicated chainings");
        }

        auto valuePromise = _promise;
        _notify = [=]{
          if (!valuePromise->hasAssigned())
            throw std::logic_error("invalid promise state");

          if (valuePromise->isExceptionCase()) {
            nextForward->reject(valuePromise->fetchException());
          } else {
            ProxyForwardPolicy<ForwardType>::forwarding(nextForward, valuePromise);
          }
        };

        _hasChained = true;

        // if already finished notify the next node now
        if (_promise->hasAssigned()) {
          _notify();
        }
      }

      template<typename T>
      void fulfill(T&& value) {
        ForwardFulfillPolicy<ForwardType>::wrappedFulfill(_promise, std::forward<T>(value));

        if (_notify) {
          _notify();
        }
      }

      void fulfill() {
        ForwardFulfillPolicy<ForwardType>::wrappedFulfill(_promise);

        if (_notify) {
          _notify();
        }
      }

      void reject(std::exception_ptr exception) {
        _promise->setException(exception);
        if (_notify) {
          _notify();
        }
      }

      bool hasChained() const {
        return _hasChained;
      }

      bool isFulfilled() const {
        return _promise->hasAssigned() && !_promise->isExceptionCase();
      }

      bool isRejected() const {
        return _promise->hasAssigned() && _promise->isExceptionCase();
      }
    };
      

    template<typename ForwardType>
    void ProxyForwardPolicy<ForwardType>::forwarding(const DeferPromiseCore<ForwardType>& core, const SharedPromiseValue<ForwardType>& promise) {
      core->fulfill(promise->value);
    }

    void ProxyForwardPolicy<void>::forwarding(const DeferPromiseCore<void>& core, const SharedPromiseValue<void>& promise) {
      core->fulfill();
    };

    template<typename ReturnType, typename ArgType, typename IsTask>
    class PromiseNodeInternalBase : public PromiseNode<ReturnType>
                                  , public Fulfill<ArgType, IsTask> {
    protected:
      using PreviousRetrievable = Fulfill<ArgType, IsTask>;

    protected:
      DeferPromiseCore<ReturnType> _forward;
      std::shared_ptr<ThreadContext> _context;

      std::function<void(std::exception_ptr)> _onReject;
      std::once_flag _called;

    public:
      PromiseNodeInternalBase(std::function<void(std::exception_ptr)>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : PromiseNode<ReturnType>()
        , PreviousRetrievable()
        , _forward{ std::make_unique<Forward<ReturnType>>() }
        , _context{ context }
        , _onReject{ std::move(onReject) }
      {}

    public:
      virtual void chainNext(const SharedNonTaskFulfill<ReturnType>& fulfill, std::function<void()>&& notify) override {
        _forward->doChaining(fulfill, std::move(notify));
      }

      virtual void chainNext(const DeferPromiseCore<ReturnType>& nextForward) override {
        _forward->doChaining(nextForward);
      }

    public:
      virtual bool isFulfilled() const override {
        return _forward->isFulfilled();
      }

      virtual bool isRejected() const override {
        return _forward->isRejected();
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

    template<typename ReturnType>
    struct RunArgFulfillPolicy {
      template<typename ForwardPointer, typename FulfillFn, typename ArgType>
      static void runFulfill(ForwardPointer& forward, FulfillFn& fulfillFn, ArgType&& value) {
        forward->fulfill(fulfillFn(std::forward<ArgType>(value)));
      }
    };

    template<>
    struct RunArgFulfillPolicy<void> {
      template<typename ForwardPointer, typename FulfillFn, typename ArgType>
      static void runFulfill(ForwardPointer& forward, FulfillFn& fulfillFn, ArgType&& value) {
        fulfillFn(std::forward<ArgType>(value));
        forward->fulfill();
      }
    };

    template<typename ReturnType>
    struct RunVoidFulfillPolicy {
      template<typename ForwardPointer, typename FulfillFn>
      static void runFulfill(ForwardPointer& forward, FulfillFn& fulfillFn) {
        forward->fulfill(fulfillFn());
      }
    };

    template<>
    struct RunVoidFulfillPolicy<void> {
      template<typename ForwardPointer, typename FulfillFn>
      static void runFulfill(ForwardPointer& forward, FulfillFn& fulfillFn) {
        fulfillFn();
        forward->fulfill();
      }
    };

    //
    // Promise node
    //
    template<typename ReturnType, typename ArgType, typename IsTask = std::false_type>
    class PromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType, std::false_type> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType, std::false_type>;

    private:
      std::function<ReturnType(ArgType)> _onFulfill;

    public:
      PromiseNodeInternal(std::function<ReturnType(ArgType)>&& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _onFulfill{ std::move(onFulfill) }
      {}

    public:
      virtual void run() override {
        std::call_once(Base::_called, [&]() {
          ArgType preValue;

          try {
            preValue = Base::PreviousRetrievable::get();
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
          RunArgFulfillPolicy<ReturnType>::runFulfill(Base::_forward, _onFulfill, std::forward<T>(preFulfilled));
        } catch (...) {
          Base::runReject();
        }
      }
    }; 

    template<typename ReturnType, typename IsTask>
    class PromiseNodeInternal<ReturnType, void, IsTask> : public PromiseNodeInternalBase<ReturnType, void, IsTask> {
      using Base = PromiseNodeInternalBase<ReturnType, void, IsTask>;

    private:
      std::function<ReturnType()> _onFulfill;

    public:
      PromiseNodeInternal(std::function<ReturnType()>&& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _onFulfill{ std::move(onFulfill) }
      {}

    public:
      virtual void run() override {
        std::call_once(Base::_called, [&]() {
          try {
            Base::PreviousRetrievable::get();
          } catch (...) {
            Base::runReject();
            return;
          }

          try {
            runFulfill();
          } catch (...) {
            // previous task is failed
            Base::runReject();
          }
        });
      }

    protected:
      void runFulfill() noexcept {
        // warpped the exception
        try {
          RunVoidFulfillPolicy<ReturnType>::runFulfill(Base::_forward, _onFulfill);
        } catch (...) {
          Base::runReject();
        }
      }
    }; 
  // end of details
  }
}

#endif // PROMISE_INTERNALS_BASE_H
