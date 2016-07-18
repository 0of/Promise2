/*
* Promise2
*
* Copyright (c) 2015 "0of" Magnus
* Licensed under the MIT license.
* https://github.com/0of/Promise2/blob/master/LICENSE
*/

#ifndef PROMISE_PUBLIC_APIS_H
#define PROMISE_PUBLIC_APIS_H

#include <functional>
#include <memory>
#include <exception>
#include <stdexcept>
 
#include "../PromiseConfig.h"
#include "../trait/declfn.h"

namespace Promise2 {

  //
  // @class Void
  //
  class Void {};

  // 
  // @trait BoxVoid UnboxVoid
  //
  // convert void to Void
  template<typename T>
  using BoxVoid = typename std::conditional_t<std::is_void<T>::value, Void, T>;

  // covert Void to void
  template<typename T>
  using UnboxVoid = typename std::conditional_t<std::is_same<T, Void>::value, void, T>;

  // declarations
  namespace Details {
    template<typename T> class PromiseNode;
    template<typename T, template<typename K> class ForwardTrait> class Forward;
    template<typename ForwardType> class SingleValueForwardTrait;
    template<typename ForwardType> class MultiValueForwardTrait;
    template<typename T> using DeferPromiseCore = std::shared_ptr<Forward<BoxVoid<T>, SingleValueForwardTrait>>;
    template<typename T> using DeferRecursionPromiseCore = std::shared_ptr<Forward<BoxVoid<T>, MultiValueForwardTrait>>;
  } // Details
    
  // !
  template<typename T> class Promise;
  template<typename T> class RecursionPromise;
  template<typename T> using SharedPromiseNode = std::shared_ptr<Details::PromiseNode<BoxVoid<T>>>;
  template<typename T> using SharedRecursionPromiseNode = std::shared_ptr<Details::RecursionPromiseNode<BoxVoid<T>>>;
  template<typename T> using OnRejectFunction = std::function<Promise<UnboxVoid<T>>(std::exception_ptr)>;
  template<typename T> using OnRecursionRejectFunction = std::function<RecursionPromise<UnboxVoid<T>>(std::exception_ptr)>;
  // !

  template<typename First, typename... Rest> using first_of = First;
  template<typename T, typename RecursionMode> using DeferCoreType = std::conditional_t<std::is_same<RecursionMode, std::true_type>::value, 
                                                                                            Details::DeferPromiseCore<T>,
                                                                                            Details::DeferPromiseCore<T>>;

  //
  // @class ThreadContext
  //
  class ThreadContext {
  public:
    virtual ~ThreadContext() = default;

  public:
    virtual void scheduleToRun(std::function<void()>&& task) = 0;
  };

  //
  // @class PromiseDeferBase
  //  expose public APIs
  //
  template<typename T, typename RecursionMode>
  class PromiseDeferBase {
  private:
    DeferCoreType<T, RecursionMode> _core;

  public:
    PromiseDeferBase(DeferCoreType<T, RecursionMode>& core);

    PromiseDeferBase(PromiseDeferBase<T, RecursionMode>&&) = default;
    ~PromiseDeferBase() = default;

  public:
    template<typename X> void setResult(X&& r);

    void setException(std::exception_ptr e);

  private:
    PromiseDeferBase(const PromiseDeferBase<T, RecursionMode>&) = delete;
    PromiseDeferBase<T, RecursionMode>& operator = (const PromiseDeferBase<T, RecursionMode>&) = delete;
  };

  template<typename RecursionMode>
  class PromiseDeferBase<void, RecursionMode> {
  private:
    DeferCoreType<void, RecursionMode> _core;

  public:
    PromiseDeferBase(DeferCoreType<void, RecursionMode>& core);

    PromiseDeferBase(PromiseDeferBase<void, RecursionMode>&&) = default;
    ~PromiseDeferBase() = default;

  public:
    void setResult();

    void setException(std::exception_ptr e);

  private:
    PromiseDeferBase(const PromiseDeferBase<void, RecursionMode>&) = delete;
    PromiseDeferBase<void, RecursionMode>& operator = (const PromiseDeferBase<void, RecursionMode>&) = delete;
  };

  //
  // @using PromiseDefer
  //
  template<typename T> using PromiseDefer = PromiseDeferBase<T, std::false_type>;

  template<typename T>
  class PromiseSpawner {
  public:
    template<typename Task>
    static Promise<T> New(Task&& task, ThreadContext* &&context) {
      auto taskFn = declfn(task){ std::move(task) };

      static_assert(!std::is_same<decltype(taskFn), std::false_type>::value, "you need to provide a callable");
      static_assert(!std::is_same<decltype(Spawn(std::move(taskFn), std::move(context))), std::false_type>::value, "match nothing...");

      return std::move(Spawn(std::move(taskFn), std::move(context)));
    }

  private:
    // constructor with task and running context
    static Promise<T> Spawn(std::function<T(void)>&& task, ThreadContext* &&context);

    // constructor with deferred task
    static Promise<T> Spawn(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context);

    // constructor with nesting promise task
    static Promise<T> Spawn(std::function<Promise<T>()>&& task, ThreadContext* &&context);

    // matching nothing
    template<typename NonMatching> static auto Spawn(NonMatching&&, ThreadContext* &&) -> std::false_type;
  };

  template<typename T>
  class PromiseResolveSpawner {
  public:
    template<typename ArgType>
    static Promise<T> Resolved(ArgType&& arg);
    static Promise<T> Rejected(std::exception_ptr e);
  };

  template<>
  class PromiseResolveSpawner<void> {
  public:
    static Promise<void> Resolved();
    static Promise<void> Rejected(std::exception_ptr e);
  };

  template<typename T>
  class PromiseThenable {
  public:
    template<typename NextT, typename ConvertibleT>
    static Promise<UnboxVoid<NextT>> Then(SharedPromiseNode<T>& node,
                               std::function<NextT(ConvertibleT)>&& onFulfill,
                               OnRejectFunction<NextT>&& onReject, 
                               ThreadContext* &&context);

    template<typename NextT, typename ConvertibleT>
    static Promise<UnboxVoid<NextT>> Then(SharedPromiseNode<T>& node,
                               std::function<Void(PromiseDefer<NextT>&&, ConvertibleT)>&& onFulfill,
                               OnRejectFunction<NextT>&& onReject, 
                               ThreadContext* &&context);

    template<typename NextT, typename ConvertibleT>
    static Promise<UnboxVoid<NextT>> Then(SharedPromiseNode<T>& node, 
                               std::function<Promise<UnboxVoid<NextT>>(ConvertibleT)>&& onFulfill,
                               OnRejectFunction<NextT>&& onReject, 
                               ThreadContext* &&context);

    // matching nothing
    template<typename NonMatching> static auto Then(...) -> std::false_type;
  };

  // allow recursion resolving
  template<typename T>
  class PromiseRecursible {
  public:
    // iterator must implements std::input_iterator_tag supported operations
    // and under multi-threads context(recursion state pass to the receiver i.e. thenable promise) equality check and increment operation must be atomic
    template<class InputIterator>
    static RecursionPromise<T> Iterate(InputIterator begin, InputIterator end);
  };

  template<typename T>
  class RecursionPromiseThenable {
  public:
    template<typename NextT, typename ConvertibleT>
    static RecursionPromise<UnboxVoid<NextT>> Then(SharedRecursionPromiseNode<T>& node,
                                                   std::function<NextT(ConvertibleT)>&& onFulfill,
                                                   OnRecursionRejectFunction<NextT>&& onReject, 
                                                   ThreadContext* &&context);
  };

#if ONREJECT_IMPLICITLY_RESOLVED
  template<typename T>
  struct OnRejectImplicitlyResolved {
    static OnRejectFunction<T> wrapped(std::function<void(std::exception_ptr)>&&);

    static OnRejectFunction<T> wrapped(std::function<Promise<T>(std::exception_ptr)>&& f) { return std::move(f); }

    static auto wrapped(...) -> std::false_type;
  };

  template<>
  struct OnRejectImplicitlyResolved<void> {
    static OnRejectFunction<void> wrapped(std::function<void(std::exception_ptr)>&&);

    static OnRejectFunction<void> wrapped(std::function<Promise<void>(std::exception_ptr)>&& f) {
      return std::move(f);
    }

    static auto wrapped(...) -> std::false_type;
  };
#endif // ONREJECT_IMPLICITLY_RESOLVED
  
  template<typename T, typename... _> struct IsKindOfDefer : public std::false_type {};
  template<typename T, typename... _> struct IsKindOfDefer<PromiseDefer<T>&&, _...> : public std::true_type {};

  // void eliminator
  struct VoidEliminator {
    template<typename ReturnType, typename... ArgsType>
    static auto eliminate(std::function<ReturnType(ArgsType...)>&&, std::enable_if_t<!IsKindOfDefer<ArgsType...>::value, Void>);

    template<typename ReturnType>
    static auto eliminate(std::function<ReturnType()>&&, Void);

    // deferred promise
    template<typename ReturnType, typename... ArgsType>
    static auto eliminate(std::function<ReturnType(ArgsType...)>&&, std::enable_if_t<IsKindOfDefer<ArgsType...>::value, Void>);
  };

  template<typename SharedPromiseNodeType>
  class GenericPromise {
  private:
    using SelfType = GenericPromise<SharedPromiseNodeType>;

  protected:
    SharedPromiseNodeType _node;

  protected:
    template<typename Thenable, typename OnFulfill, typename OnReject>
    auto then(OnFulfill&& onFulfill,
              OnReject&& onReject, 
              ThreadContext* &&context) {
      static_assert(!std::is_same<declfn(onFulfill), std::false_type>::value &&
                    !std::is_same<declfn(onReject), std::false_type>::value , "you need to provide a callable");

      auto onFulfillFn = VoidEliminator::eliminate(declfn(onFulfill){ std::move(onFulfill) }, Void{});

#if ONREJECT_IMPLICITLY_RESOLVED
      auto onRejectFn = OnRejectImplicitlyResolved<typename declfn(onFulfill)::result_type>::wrapped(declfn(onReject) { std::move(onReject) });
#else
      auto onRejectFn = declfn(onReject) { std::move(onReject) };
#endif // ONREJECT_IMPLICITLY_RESOLVED

      static_assert(!std::is_same<decltype(Thenable::Then(_node, std::move(onFulfillFn), std::move(onRejectFn), std::move(context))), std::false_type>::value, "match nothing...");

      if (!isValid()) throw std::logic_error("invalid promise");
      return Thenable::Then(_node, std::move(onFulfillFn), std::move(onRejectFn), std::move(context));
    }

  public:
    bool isValid() const {
      return !!_node;
    }

  public:
    bool isFulfilled() const; 
    bool isRejected() const;

  public:
    inline SharedPromiseNodeType internal() const { return _node; }
  };

  //
  // @class Promise
  //
  template<typename T>
  class Promise : public GenericPromise<SharedPromiseNode<T>>, 
                  public PromiseSpawner<T>, 
                  public PromiseResolveSpawner<T> {
    template<typename Type> friend class PromiseThenable;
    template<typename Type> friend class PromiseResolveSpawner;

  private:
    using Base = GenericPromise<SharedPromiseNode<T>>;
    using Thenable = PromiseThenable<BoxVoid<T>>;
    using SelfType = Promise<T>;

    friend class PromiseSpawner<T>;

  public:
    using PromiseType = T;

  public:
    // empty constructor
    Promise() = default;

    // copy constructor
    Promise(const SelfType& ) = default;
    // move constructor
    Promise(SelfType&& ) = default;

    SelfType& operator = (const SelfType& ) = default;
    SelfType& operator = (SelfType&& ) = default;

  public:
    template<typename OnFulfill, typename OnReject>
    auto then(OnFulfill&& onFulfill,
              OnReject&& onReject, 
              ThreadContext* &&context) {
      return Base::then<Thenable>(std::forward<OnFulfill>(onFulfill), std::forward<OnReject>(onReject), std::move(context));
    }
  };
}
 
#endif // PROMISE_PUBLIC_APIS_H
 