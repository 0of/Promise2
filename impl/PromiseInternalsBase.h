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
    // each `Promise` will hold one shared `PromiseNode`
    //
    template<typename T>
    class PromiseNode {
    public:
      virtual ~PromiseNode() = default;

    public:
      virtual void chainNext(std::function<void(const SharedPromiseValue<T>&)>&& notify) = 0;
      // proxy fowarding
      virtual void chainNext(const DeferPromiseCore<T>&) = 0;

    public:
      virtual bool isFulfilled() const = 0;
      virtual bool isRejected() const = 0;
    };

    //
    // fulfillment
    //  SharedPromiseValue wrapper
    //
    template<typename FulfillArgType, typename IsTask>
    class Fulfillment {
    private:
      SharedPromiseValue<FulfillArgType> _previousPromise;

    public:
      Fulfillment(const SharedPromiseValue<FulfillArgType>& v)
        : _previousPromise{ v }
      {}

      ~Fulfillment() = default;

    public:
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

    template<typename FulfillArgType>
    class Fulfillment<FulfillArgType, std::true_type> {
    public:
      Fulfillment(const SharedPromiseValue<FulfillArgType>& _)
      {}

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
      enum class Status : std::uint8_t {
        Running = 0,
        Fulfilled = 1,
        Rejected = 2
      };

    private:
      std::function<void(const SharedPromiseValue<ForwardType>&)> _forwardNotify;

#ifdef DEBUG
      std::atomic_flag _chainingGuard;
#endif // DEBUG

      std::atomic_bool _hasChained;

      std::atomic<SharedPromiseValue<ForwardType> *> _aboutToForwardValue;

      Status _status;

    public:
      Forward()
        : _forwardNotify{}
#ifdef DEBUG
        , _chainingGuard{ ATOMIC_FLAG_INIT }
#endif // DEBUG
        , _hasChained{ false }
        , _aboutToForwardValue{ new SharedPromiseValue<ForwardType>{ std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>() } }
        , _status { Status::Running }
      {}
      ~Forward() {
        auto value = _aboutToForwardValue.exchange(nullptr);
        if (value) {
          delete value;
        }
      }

    public:
      void doChaining(const DeferPromiseCore<ForwardType>& nextForward) {
#ifdef DEBUG
        if (_chainingGuard.test_and_set()) {
          // already chained or is chaining
          throw std::logic_error("promise duplicated chainings");
        }
#endif // DEBUG

        // move the notify before enter the critical section
        _forwardNotify = [=](const SharedPromiseValue<ForwardType>& valuePromise){
          if (!valuePromise->hasAssigned())
            throw std::logic_error("invalid promise state");

          if (valuePromise->isExceptionCase()) {
            nextForward->reject(valuePromise->fetchException());
          } else {
            nextForward->fulfill(valuePromise->template getValue<ForwardType>());
          }
        };

        chaining();
      }


      void doChaining(std::function<void(const SharedPromiseValue<ForwardType>&)>&& notify) {
#ifdef DEBUG
        if (_chainingGuard.test_and_set()) {
          // already chained or is chaining
          throw std::logic_error("promise duplicated chainings");
        }
#endif // DEBUG

        // move the notify before enter the critical section
        _forwardNotify = std::move(notify);

        chaining();
      }

      template<typename T>
      void fulfill(T&& value) {
        // acquire
        auto prevValue = _aboutToForwardValue.exchange(nullptr);

        // previous value has been assigned as null, which means already chained
        if (!prevValue) {
          // just fire the notification with forward notifier
          auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
          sharedValue->setValue(std::forward<T>(value));

          _status = Status::Fulfilled;

          std::atomic_thread_fence(std::memory_order_acquire);

          _forwardNotify(sharedValue);

        } else {
          // epoch before chained
          // store the value for future notification
          (*prevValue)->setValue(std::forward<T>(value));

          // exchange back to prevous value cuz `doChaining` may be waiting for the condition desperately
          _aboutToForwardValue.exchange(prevValue);

          _status = Status::Fulfilled;
        }
      }
        
      void reject(std::exception_ptr exception) {

         // acquire
        auto prevValue = _aboutToForwardValue.exchange(nullptr);

        // previous value has been assigned as null, which means already chained
        if (!prevValue) {
          // just fire the notification with forward notifier
          auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
          sharedValue->setException(exception);

          _status = Status::Rejected;

          std::atomic_thread_fence(std::memory_order_acquire);

          _forwardNotify(sharedValue);

        } else {
          // epoch before chained
          // store the value for future notification
          (*prevValue)->setException(exception);

          // exchange back to prevous value cuz `doChaining` may be waiting for the condition desperately
          _aboutToForwardValue.exchange(prevValue);

          _status = Status::Rejected;
        }
      }

      bool hasChained() const {
        return _hasChained;
      }

      bool isFulfilled() const {
        return _status == Status::Fulfilled;
      }

      bool isRejected() const {
        return _status == Status::Rejected;
      }

    private:
      // thread safe chaining
      void chaining() {
        while (true) {
          // acquire only if not null
          auto prevValue = _aboutToForwardValue.exchange(nullptr);

          // means `fulfill/reject` has acquired ownership
          if (!prevValue) {
            std::this_thread::yield();
            continue;
          }

          // ownership acquired 
          // notify with previously stored value
          if ((*prevValue)->hasAssigned()) {
            _forwardNotify(*prevValue);
          }

          // release the object for being no useful anymore
          delete prevValue;

          break;
        }
      }
    };

    template<typename ReturnType, typename ArgType, typename IsTask>
    class PromiseNodeInternalBase : public PromiseNode<ReturnType> {
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
        , _forward{ std::make_unique<Forward<ReturnType>>() }
        , _context{ context }
        , _onReject{ std::move(onReject) }
      {}

    public:
      virtual void chainNext(const DeferPromiseCore<ReturnType>& nextForward) override {
        _forward->doChaining(nextForward);
      }

      virtual void chainNext(std::function<void(const SharedPromiseValue<ReturnType>&)>&& notify) override {
        _forward->doChaining(std::move(notify));
      }

    public:
      virtual bool isFulfilled() const override {
        return _forward->isFulfilled();
      }

      virtual bool isRejected() const override {
        return _forward->isRejected();
      }

    public:
      void runWith(const SharedPromiseValue<ArgType>& value) {
        Fulfillment<ArgType, IsTask> fulfillment { value };
        try {
          fulfillment.guard();
        } catch (...) {
          this->runReject();
          return;
        }

        this->onRun(fulfillment);
      }

      // start as task
      //  if non task call this function, guard() will throw logic exception
      void start() {
        this->runWith(nullptr);
      }

    protected:
      virtual void onRun(Fulfillment<ArgType, IsTask>& fulfillment) noexcept {}

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
      virtual void onRun(Fulfillment<ArgType, IsTask>& fulfillment) noexcept override {
        try {
          Base::_forward->fulfill(
            _onFulfill(fulfillment.template get<ConvertibleArgType>())
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
