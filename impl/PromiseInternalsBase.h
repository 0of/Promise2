#ifndef PROMISE_INTERNALS_BASE_H
#define PROMISE_INTERNALS_BASE_H

#include <thread>
#include <memory>
#include <type_traits>
#include <future>
#include <vector>

#include "value/GeneralPromiseValue.h"

namespace Promise2 {
  namespace Details {

    //
    //  @enums
    //
    enum class PormiseMode {
      SingleValue
    };

    enum class Status : std::uint16_t {
      Running = 0,
      Fulfilled = 1,
      Rejected = 2
    };

    enum class ChainedFlag : std::uint16_t {
      No = 0,
      Yes = 1
    };

    enum class RecursionStatus : std::uint16_t {
      Loopping = 0,
      Finished = 1,
      ExceptionOccurred = 2,
      Fired = 3
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
    // each `RecursionPromise` will hold one shared `RecursionPromiseNode`
    //
    template<typename T>
    class RecursionPromiseNode {
    public:
      virtual ~RecursionPromiseNode() = default;

    public:
      virtual void chainRecursionNext(std::function<void(const SharedPromiseValue<T>&)>&& notify, 
                             std::function<void(const SharedPromiseValue<Void>&)>&& notifyWhenFinished) = 0;

      virtual void chainRecursionNext(const DeferRecursionPromiseCore<T>&) = 0;

    public:
      virtual void chainNext(std::function<void(const SharedPromiseValue<Void>&)>&& notify) = 0;

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

    template<typename ForwardType>
    class SingleValueForwardTrait {
    private:
      SharedPromiseValue<ForwardType> *_aboutToForwardValue;

    public:
      SingleValueForwardTrait()
        : _aboutToForwardValue{ nullptr }
      {}

    public:
      void onDestructing() {
        if (_aboutToForwardValue) {
          delete _aboutToForwardValue;
          _aboutToForwardValue = nullptr;
        }
      }

      template<typename T>
      void onFulfillBeforeChain(T&& v) {
        auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
        sharedValue->setValue(std::forward<T>(v));

        _aboutToForwardValue = new SharedPromiseValue<ForwardType>{ sharedValue };
      }

      void onExceptionBeforeChain(std::exception_ptr e) {
        auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
        sharedValue->setException(e);

        _aboutToForwardValue = new SharedPromiseValue<ForwardType>{ sharedValue };
      }

      void onChaining(std::function<void(const SharedPromiseValue<ForwardType>&)>& notify) {
        if (_aboutToForwardValue) {
          notify(*_aboutToForwardValue);
            
          // release the object for being no useful anymore
          delete _aboutToForwardValue;

          _aboutToForwardValue = nullptr;
        }
      }
    };

    template<typename ForwardType>
    class MultiValueForwardTrait {
    private:
      // notify order relaxed
      std::vector<SharedPromiseValue<ForwardType>> *_aboutToForwardValueSet;

    public:
      MultiValueForwardTrait()
        : _aboutToForwardValueSet{ nullptr }
      {}

    public:
      void onDestructing() {
        if (_aboutToForwardValueSet) {
          delete _aboutToForwardValueSet;
          _aboutToForwardValueSet = nullptr;
        }
      }

      template<typename T>
      void onFulfillBeforeChain(T&& v) {
        auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
        sharedValue->setValue(std::forward<T>(v));

        if (!_aboutToForwardValueSet) {
          _aboutToForwardValueSet = new std::vector<SharedPromiseValue<ForwardType>>{ sharedValue };
        } else {
          _aboutToForwardValueSet->push_back(sharedValue);
        }
      }

      void onExceptionBeforeChain(std::exception_ptr e) {
        auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
        sharedValue->setException(e);

        if (!_aboutToForwardValueSet) {
          _aboutToForwardValueSet = new std::vector<SharedPromiseValue<ForwardType>>{ sharedValue };
        } else {
          _aboutToForwardValueSet->push_back(sharedValue);
        }
      }

      void onChaining(std::function<void(const SharedPromiseValue<ForwardType>&)>& notify) {
        if (_aboutToForwardValueSet) {

          for (auto& eachValue :  *_aboutToForwardValueSet) {
            notify(eachValue);
          }
          
          // release the object for being no useful anymore
          delete _aboutToForwardValueSet;

          _aboutToForwardValueSet = nullptr;
        }
      }
    };

    //
    // forward traits
    //
    template<typename ForwardType, template<typename T> class ForwardTrait>
    class Forward {
    private:
      std::atomic<ChainedFlag> _chainedFlag;
      Status _status;

    protected:
      std::function<void(const SharedPromiseValue<ForwardType>&)> _forwardNotify;

    private:
      ForwardTrait<ForwardType> _forwardTrait;
   
    public:
      Forward()
        : _chainedFlag{ ChainedFlag::No }
        , _status { Status::Running }
        , _forwardNotify{}
      {}

      virtual ~Forward() {
        if (_chainedFlag.load() == ChainedFlag::No) {
          _forwardTrait.onDestructing();
        }
      }

    public:
      virtual void doChaining(const DeferPromiseCore<ForwardType>& nextForward) {
        // move the notify before enter the critical section
        _forwardNotify = std::move(getDeferForwardNotify(nextForward));

        chaining();
      }

      virtual void doChaining(std::function<void(const SharedPromiseValue<ForwardType>&)>&& notify) {
        // move the notify before enter the critical section
        _forwardNotify = std::move(notify);

        chaining();
      }

      template<typename T>
      void fulfill(T&& value) {
        // acquire ownership
        auto preFlag = _chainedFlag.exchange(ChainedFlag::Yes);

        // already chained
        if (ChainedFlag::Yes == preFlag) {
          // just fire the notification with forward notifier
          auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
          sharedValue->setValue(std::forward<T>(value));

          _status = Status::Fulfilled;

          this->notify(sharedValue);

        } else {
          // epoch before chained
          // store the value for future notification
          _forwardTrait.onFulfillBeforeChain(std::forward<T>(value));

          // exchange back to prevous flag cuz `doChaining` may be waiting for the condition desperately
          _chainedFlag.store(ChainedFlag::No);

          _status = Status::Fulfilled;
        }
      }
        
      void reject(std::exception_ptr exception) {
        // acquire ownership
        auto preFlag = _chainedFlag.exchange(ChainedFlag::Yes);

        // already chained
        if (ChainedFlag::Yes == preFlag) {
          // just fire the notification with forward notifier
          auto sharedValue = std::make_shared<typename SharedPromiseValue<ForwardType>::element_type>();
          sharedValue->setException(exception);

          _status = Status::Rejected;

          this->notify(sharedValue);

        } else {
          // epoch before chained
          // store the value for future notification
          _forwardTrait.onExceptionBeforeChain(exception);

          // exchange back to prevous flag cuz `doChaining` may be waiting for the condition desperately
          _chainedFlag.store(ChainedFlag::No);

          _status = Status::Rejected;
        }
      }

      bool hasChained() const {
        return _chainedFlag.load();
      }

      bool isFulfilled() const {
        return _status == Status::Fulfilled;
      }

      bool isRejected() const {
        return _status == Status::Rejected;
      }

    protected:
      virtual void notify(const SharedPromiseValue<ForwardType>& value) {
        std::atomic_thread_fence(std::memory_order_acquire);
        _forwardNotify(value);
      }

      auto getDeferForwardNotify(const DeferPromiseCore<ForwardType>& nextForward) {
        return [=](const SharedPromiseValue<ForwardType>& valuePromise){
          if (!valuePromise->hasAssigned())
            throw std::logic_error("invalid promise state");

          if (valuePromise->isExceptionCase()) {
            nextForward->reject(valuePromise->fetchException());
          } else {
            nextForward->fulfill(valuePromise->template getValue<ForwardType>());
          }
        };
      }

    private:
      // thread safe chaining
      void chaining() {
        while (true) {
          // acquire only if is `no` flag
          auto preFlag = _chainedFlag.exchange(ChainedFlag::Yes);

          // means `fulfill/reject` has acquired ownership
          if (ChainedFlag::Yes == preFlag) {
            std::this_thread::yield();
            continue;
          }

          // ownership acquired 
          // notify with previously stored value
          _forwardTrait.onChaining(_forwardNotify);

          break;
        }
      }
    };

    template<typename ForwardType, template<typename T> class ForwardTrait>
    class MultiChainForward : public Forward<ForwardType, ForwardTrait> {
      using Base = Forward<ForwardType, ForwardTrait>;

    private:
      std::atomic<std::uint32_t> _flags;

    private:
      class ChainingMutex {
      private:
        std::atomic<std::uint32_t> *_flags;

      public:
        // modified when lock
        bool alreadyChained;

      public:
        ChainingMutex(std::atomic<std::uint32_t> *flags)
          : _flags{ flags }
          , alreadyChained{ false }
        {}

      public:
        void lock() {
          std::uint32_t flag = 0;

          while ((flag = _flags->fetch_or(0x1)) & 0x1) {
            std::this_thread::yield();
          }

          alreadyChained = (flag & 0x10) == 0x10;
        }

        void unlock() noexcept {
          _flags->fetch_and(0x10);
        }
      };

      class NotifyMutex {
      private:
        std::atomic<std::uint32_t> *_flags;

      public:
        NotifyMutex(std::atomic<std::uint32_t> *flags)
          : _flags{ flags }
        {}

      public:
        void lock() {
          while (_flags->fetch_or(0x1) & 0x1) {
            std::this_thread::yield();
          }
        }

        void unlock() noexcept {
          _flags->fetch_and(0x10);
        }
      };

    public:
      virtual void doChaining(const DeferPromiseCore<ForwardType>& nextForward) {
        ChainingMutex mutex{ &_flags };
        std::lock_guard<ChainingMutex> _{ mutex };

        if (mutex.alreadyChained) {
          Base::_forwardNotify = std::move(this->getDeferForwardNotify(nextForward));
        } else {
          Base::doChaining(nextForward);
        }
      }

      virtual void doChaining(std::function<void(const SharedPromiseValue<ForwardType>&)>&& notify) {
        ChainingMutex mutex{ &_flags };
        std::lock_guard<ChainingMutex> _{ mutex };

        if (mutex.alreadyChained) {
          Base::_forwardNotify = std::move(notify);
        } else {
          Base::doChaining(std::move(notify));
        }
      }

    protected:
      virtual void notify(const SharedPromiseValue<ForwardType>& value) {
        NotifyMutex mutex{ &_flags };
        std::lock_guard<NotifyMutex> _{ mutex };

        this->_forwardNotify(value);
      }
    };

    template<typename ReturnType, typename ArgType, typename IsTask>
    class PromiseNodeInternalBase : public PromiseNode<ReturnType> {
    protected:
      DeferPromiseCore<ReturnType> _forward;
      std::shared_ptr<ThreadContext> _context;

      OnRejectFunction<ReturnType> _onReject;
      
    public:
      PromiseNodeInternalBase(OnRejectFunction<ReturnType>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : PromiseNode<ReturnType>()
        , _forward{ std::make_unique<Forward<ReturnType, SingleValueForwardTrait>>() }
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
            // chain the returned promise only if valid 
            auto p = _onReject(std::current_exception());
            if (p.isValid()) {
              p.internal()->chainNext(_forward);
            } else {
              _forward->reject(std::current_exception());
            }
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
    // RecursionPromiseNodeInternalBase
    //
    template<typename ReturnType, typename ArgType, typename IsTask>
    class RecursionPromiseNodeInternalBase : public RecursionPromiseNode<ReturnType> {
    protected:
      DeferRecursionPromiseCore<ReturnType> _forward;
      DeferPromiseCore<Void> _finishForward;
      std::shared_ptr<ThreadContext> _context;

      OnRecursionRejectFunction<ReturnType> _onReject;

    protected:
      RecursionPromiseNodeInternalBase(OnRecursionRejectFunction<ReturnType>&& onReject, 
                                       const std::shared_ptr<ThreadContext>& context)
        : RecursionPromiseNode<ReturnType>()
        , _forward{ std::make_unique<Forward<ReturnType, MultiValueForwardTrait>>() }
        , _finishForward{ std::make_unique<MultiChainForward<Void, SingleValueForwardTrait>>() }
        , _context{ context }
        , _onReject{ std::move(onReject) }
      {}

    public:
      virtual void chainRecursionNext(std::function<void(const SharedPromiseValue<ReturnType>&)>&& notify,
                                      std::function<void(const SharedPromiseValue<Void>&)>&& notifyWhenFinished) override {

        _forward->doChaining(std::move(notify));
        _finishForward->doChaining(std::move(notifyWhenFinished));
      }

      virtual void chainRecursionNext(const DeferRecursionPromiseCore<ReturnType>& defer) override {
        throw 0;
      }

    public:
      virtual void chainNext(std::function<void(const SharedPromiseValue<Void>&)>&& notify) override {
        _finishForward->doChaining(std::move(notify));
      }

    public:
      virtual bool isFulfilled() const override {
        return _finishForward->isFulfilled();
      }

      virtual bool isRejected() const override {
        return _finishForward->isRejected();
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

      void finish(const SharedPromiseValue<Void>& value) {
        Fulfillment<Void, std::false_type> fulfillment { value };
        try {
          fulfillment.guard();
        } catch (...) {
          _finishForward->reject(std::current_exception());
          return;
        }

        _finishForward->fulfill(Void{});
      }

    protected:
      virtual void onRun(Fulfillment<ArgType, IsTask>& fulfillment) noexcept {}

      // called when exception has been thrown
      void runReject() noexcept {
        if (_onReject) {
          try {
            // chain the returned promise only if valid 
            auto p = _onReject(std::current_exception());
            if (p.isValid()) {
              p.internal()->chainRecursionNext(_forward);
            } else {
              _forward->reject(std::current_exception());
            }
          } catch (...) {
            _forward->reject(std::current_exception());
          }
        } else {
          _forward->reject(std::current_exception());
        }
      }
    };

    //
    // Promise node
    //
    template<typename ReturnType, typename ArgType, typename ConvertibleArgType, typename IsTask = std::false_type, bool IsRecursion = false>
    class PromiseNodeInternal : public std::conditional_t<IsRecursion, 
                                                         RecursionPromiseNodeInternalBase<ReturnType, ArgType, IsTask>,
                                                         PromiseNodeInternalBase<ReturnType, ArgType, IsTask>> {
      using Base = std::conditional_t<IsRecursion,
                                     RecursionPromiseNodeInternalBase<ReturnType, ArgType, IsTask>,
                                     PromiseNodeInternalBase<ReturnType, ArgType, IsTask>>;

    private:
      std::function<ReturnType(ConvertibleArgType)> _onFulfill;

    public:
      template<typename OnRejectFn>
      PromiseNodeInternal(std::function<ReturnType(ConvertibleArgType)>&& onFulfill,
                          OnRejectFn&& onReject,
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
