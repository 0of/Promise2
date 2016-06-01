#ifndef PROMISE_INTERNALS_BASE_H
#define PROMISE_INTERNALS_BASE_H

#include <thread>
#include <memory>
#include <future>
#include <type_traits>

#include "value/GeneralPromiseValue.h"

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

    //
    //  @alias
    //
    template<typename T> using SharedPromiseValue = std::shared_ptr<
      std::conditional_t<std::is_pointer<T>::value,
                         PromisePointerValue<T>,
                         std::conditional_t<std::is_reference<T>::value,
                                            PromiseRefValue<T>,
                                            PromiseValue<T>>>
    >;

    //
    // fulfill traits
    //
    template<typename FulfillArgType, typename IsTask>
    class Fulfill {
    private:
      SharedPromiseValue<FulfillArgType> _previousPromise;
      std::atomic_flag _attachGuard;

    protected:
      Fulfill()
        : _previousPromise{}
        , _attachGuard{ ATOMIC_FLAG_INIT }
      {}

    public:
      virtual ~Fulfill() = default;

    public:
      // 
      // `attach` & `guard` will never be called in concurrency
      // `attach` may be falsely invoked multi-times in mutli-thread environment
      //
      void attach(const SharedPromiseValue<FulfillArgType>& previousPromise) {
        if (_attachGuard.test_and_set()) {
          // already attached or is attaching
          throw std::logic_error("promise duplicated attachments");
        }

        _previousPromise = previousPromise;
      }

      void guard() {
        if (!_previousPromise) {
          throw std::logic_error("null promise value");
        }

        _previousPromise->accessGuard();
      }

      template<typename T>
      inline T get() {
        return std::forward<T>(_previousPromise->template getValue<T>());
      }
    };

    template<>
    class Fulfill<Void, std::true_type> {
    protected:
      Fulfill()
      {}
        
    public:
      virtual ~Fulfill() = default;
        
    public:
      void attach(const SharedPromiseValue<Void>& previousPromise) {
          throw std::logic_error("task cannot be chained");
      }
      
      void guard() {}

      template<typename T>
      inline T get() {
        return Void{};
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
            nextForward->fulfill(valuePromise->template getValue<ForwardType>());
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
        _promise->setValue(std::forward<T>(value));

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

    template<typename ReturnType, typename ArgType, typename IsTask>
    class PromiseNodeInternalBase : public PromiseNode<ReturnType>
                                  , public Fulfill<ArgType, IsTask> {
    protected:
      using PreviousRetrievable = Fulfill<ArgType, IsTask>;

    protected:
      DeferPromiseCore<ReturnType> _forward;
      std::shared_ptr<ThreadContext> _context;

      OnRejectFunction<ReturnType> _onReject;

    private:
      std::once_flag _called;

    public:
      PromiseNodeInternalBase(OnRejectFunction<ReturnType>&& onReject,
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
      virtual void run() override final {
        std::call_once(_called, [this]() {
          try {
            this->guard();
          } catch (...) {
            this->runReject();
            return;
          }

          this->onRun();
        });
      }

      virtual void onRun() noexcept {}

      // called when exception has been thrown
      void runReject() noexcept {
        if (_onReject) {
          try {
            // chain the returned promise
            _onReject(std::current_exception()).internal()->chainNext(_forward);
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
    template<typename ReturnType, typename ArgType, typename ConvertibleArgType, typename IsTask = std::false_type>
    class PromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType, IsTask> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType, IsTask>;

    private:
      std::function<ReturnType(ConvertibleArgType)> _onFulfill;

    public:
      PromiseNodeInternal(std::function<ReturnType(ConvertibleArgType)>&& onFulfill,
                  OnRejectFunction<ReturnType>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _onFulfill{ std::move(onFulfill) }
      {}

    protected:
      virtual void onRun() noexcept override {
        try {
          Base::_forward->fulfill(
            _onFulfill(Base::template get<ConvertibleArgType>())
          );
        } catch (...) {
          Base::_forward->reject(std::current_exception());
        }
      }
    }; 
  // end of details
  }
}

#endif // PROMISE_INTERNALS_BASE_H
