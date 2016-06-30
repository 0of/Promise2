#ifndef PROMISE_PUBLIC_AP_ISIMPL_H
#define PROMISE_PUBLIC_AP_ISIMPL_H

#include "../public/PromisePublicAPIs.h"
#include "PromiseInternalsBase.h"
#include "../trait/void_traits.h"

#if DEFERRED_PROMISE
#include "DeferredPromiseInternals.h"
#endif

#if NESTING_PROMISE
#include "NestingPromiseInternals.h"
#endif

#include "ResolvedRejectedPromiseInternals.h"

namespace Promise2 {

  template<typename T> struct falsehood : public std::false_type {};

#if ONREJECT_IMPLICITLY_RESOLVED
  template<typename T>
  OnRejectFunction<T> OnRejectImplicitlyResolved<T>::wrapped(std::function<void(std::exception_ptr)>&& f) {
    OnRejectFunction<T> wrapped = [reject = move(f)](std::exception_ptr e){
      reject(e);
      return Promise<T>::Resolved(T());
    };
    return std::move(wrapped);
  }

  OnRejectFunction<void> OnRejectImplicitlyResolved<void>::wrapped(std::function<void(std::exception_ptr)>&& f) {
    OnRejectFunction<void> wrapped = [reject = move(f)](std::exception_ptr e){
      reject(e);
      return Promise<void>::Resolved();
    };
    return std::move(wrapped);
  }
#endif // ONREJECT_IMPLICITLY_RESOLVED

  template<typename PredType, typename ReturnType, typename... ArgTypes>
  auto eliminateVoid(std::function<ReturnType(ArgTypes...)>&& f) {
    return std::move(VoidTrait::ArgVoid<typename PredType::template type<ArgTypes...>>::currying(
      VoidTrait::ReturnVoid<ReturnType>::currying(std::move(f))
    ));
  }

  template<typename PredType, typename ReturnType>
  auto eliminateVoid(std::function<ReturnType()>&& f) {
    return std::move(VoidTrait::ArgVoid<typename PredType::template type<void>>::currying(
      VoidTrait::ReturnVoid<ReturnType>::currying(std::move(f))
    ));
  }

/**
 * Implementations
 */
#define NEW_IMP(internal, ArgPred) \
   { using Internal = internal<BoxVoid<T>, Void, Void, std::true_type>; \
   Promise<T> spawned; \
   auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context)); \
   auto node = std::make_shared<Internal>(eliminateVoid<ArgPred>(std::move(task)), \
                  std::function<Promise<T>(std::exception_ptr)>(), sharedContext); \
   auto runnable = std::bind(&Internal::start, node); \
   sharedContext->scheduleToRun(std::move(runnable)); \
   \
   spawned._node = node; \
   return spawned; }

#define THEN_IMP(internal, T, ConvertibleT, ArgPred) \
  { static_assert(std::is_convertible<T, ConvertibleT>::value, "implicitly argument type conversion failed"); \
    using Internal = internal<BoxVoid<NextT>, BoxVoid<T>, BoxVoid<ConvertibleT>>; \
    auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context)); \
    auto nextNode = std::make_shared<Internal>(std::move(onFulfill), std::move(onReject), sharedContext); \
    node->chainNext([=](const Details::SharedPromiseValue<BoxVoid<T>>& v) { \
      auto runnable = std::bind(&Internal::runWith, nextNode, v); \
      sharedContext->scheduleToRun(std::move(runnable)); \
    }); \
    \
    Promise<UnboxVoid<NextT>> nextPromise; \
    nextPromise._node = nextNode; \
    return nextPromise; }

#define CALL_NODE_IMP(method) \
    { if (!_node) { \
        throw std::logic_error("invalid promise"); \
      } \
      return _node->method(); }

  // predicates it is necessary to append `Void` type to argument list
  struct ArgTypePred {
    template<typename... Args>
    using type = typename VoidTrait::LastType<Args...>::Type;
  };

   // `void` type if last argument type is equal to given one, which means it should pad an `Void` type to the argument list
  template<typename GivenArgType, typename...>
  struct GivenArgTypePred {
    template<typename... Args>
    using type = std::conditional_t<std::is_same<VoidTrait::last_of<Args...>, GivenArgType>::value, void, VoidTrait::last_of<Args...>>;
  };

  template<typename ReturnType, typename... ArgsType>
  auto VoidEliminator::eliminate(std::function<ReturnType(ArgsType...)>&& f, std::enable_if_t<!IsKindOfDefer<ArgsType...>::value, Void>) {
    return eliminateVoid<ArgTypePred>(std::move(f));
  }

  template<typename ReturnType>
  auto VoidEliminator::eliminate(std::function<ReturnType()>&& f, Void) {
    return eliminateVoid<ArgTypePred>(std::move(f));
  }

  template<typename ReturnType, typename... ArgsType>
  auto VoidEliminator::eliminate(std::function<ReturnType(ArgsType...)>&& f, std::enable_if_t<IsKindOfDefer<ArgsType...>::value, Void>) {
    return eliminateVoid<GivenArgTypePred<ArgsType...>>(std::move(f));
  }

  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<T(void)>&& task, ThreadContext* &&context) 
    NEW_IMP(Details::PromiseNodeInternal, ArgTypePred)

#if DEFERRED_PROMISE
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context) 
    NEW_IMP(Details::DeferredPromiseNodeInternal, GivenArgTypePred<PromiseDefer<T>&&>)
#else
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<void(PromiseDefer<T>&&)>&&, ThreadContext* &&) {
    static_assert(falsehood<T>::value, "please enable `DEFERRED_PROMISE` in `PromiseConfig.h`");
  }
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<Promise<T>()>&& task, ThreadContext* &&context) 
    NEW_IMP(Details::NestingPromiseNodeInternal, ArgTypePred)
#else
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<Promise<T>()>&&, ThreadContext* &&) {
    static_assert(falsehood<T>::value, "please enable `NESTING_PROMISE` in `PromiseConfig.h`");
  }
#endif // NESTING_PROMISE

  template<typename T>
  template<typename NextT, typename ConvertibleT>
  Promise<UnboxVoid<NextT>> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<NextT(ConvertibleT)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context) 
    THEN_IMP(Details::PromiseNodeInternal, T, ConvertibleT, ArgTypePred)

#if DEFERRED_PROMISE
  template<typename T>
  template<typename NextT, typename ConvertibleT>
  Promise<UnboxVoid<NextT>> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<Void(PromiseDefer<NextT>&&, ConvertibleT)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context)
    THEN_IMP(Details::DeferredPromiseNodeInternal, T, ConvertibleT, GivenArgTypePred<PromiseDefer<NextT>&&>)
#else
  template<typename T>
  template<typename NextT, typename ConvertibleT>
  Promise<UnboxVoid<NextT>> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<void(PromiseDefer<NextT>&&, ConvertibleT)>&& onFulfill,
                             OnRejectFunction&& onReject, 
                             ThreadContext* &&context) {
    static_assert(falsehood<NextT>::value, "please enable `DEFERRED_PROMISE` in `PromiseConfig.h`");
  }
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
  template<typename T>
  template<typename NextT, typename ConvertibleT>
  Promise<UnboxVoid<NextT>> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<Promise<UnboxVoid<NextT>>(ConvertibleT)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context)
    THEN_IMP(Details::NestingPromiseNodeInternal, T, ConvertibleT, ArgTypePred)
#else
  template<typename T>
  template<typename NextT, typename ConvertibleT>
  Promise<UnboxVoid<NextT>> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<Promise<UnboxVoid<NextT>>(ConvertibleT)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context) {
    static_assert(falsehood<NextT>::value, "please enable `NESTING_PROMISE` in `PromiseConfig.h`");
  }
#endif // NESTING_PROMISE

  template<typename T> bool Promise<T>::isFulfilled() const CALL_NODE_IMP(isFulfilled)
  template<typename T> bool Promise<T>::isRejected() const CALL_NODE_IMP(isRejected)

  template<typename T>
  template<typename ArgType>
  Promise<T> PromiseResolveSpawner<T>::Resolved(ArgType&& arg) {
    Promise<T> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<T>>(std::forward<ArgType>(arg));
    spawned._node = node;

    return spawned;
  }

  template<typename T>
  Promise<T> PromiseResolveSpawner<T>::Rejected(std::exception_ptr e) {
    Promise<T> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<T>>(e);
    spawned._node = node;

    return spawned;
  }

  Promise<void> PromiseResolveSpawner<void>::Resolved() {
    Promise<void> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<Void>>(Void{});
    spawned._node = node;

    return spawned;
  }

  Promise<void> PromiseResolveSpawner<void>::Rejected(std::exception_ptr e) {
    Promise<void> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<Void>>(e);
    spawned._node = node;

    return spawned;
  }
} // Promise2

#endif // PROMISE_PUBLIC_AP_ISIMPL_H
