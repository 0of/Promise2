#ifndef PROMISE_PUBLIC_AP_ISIMPL_H
#define PROMISE_PUBLIC_AP_ISIMPL_H

#include "PromisePublicAPIs.h"
#include "PromiseInternalsBase.h"

#if DEFERRED_PROMISE
#include "DeferredPromiseInternals.h"
#endif

#if NESTING_PROMISE
#include "NestingPromiseInternals.h"
#endif

#include "ResolvedRejectedPromiseInternals.h"

namespace Promise2 {

  template<typename T> struct falsehood : public std::false_type {};

/**
 * Implementations
 */
#define NEW_IMP(internal) \
   { Promise<T> spawned; \
   auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context)); \
   auto node = std::make_shared<internal<T, void, std::true_type>>(std::move(task), \
                  std::function<Promise<T>(std::exception_ptr)>(), sharedContext); \
   auto runnable = std::bind(&Details::PromiseNode<T>::run, node); \
   sharedContext->scheduleToRun(std::move(runnable)); \
   \
   spawned._node = node; \
   return spawned; }

#define THEN_IMP(internal, T) \
  { auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context)); \
    \
    auto nextNode = std::make_shared<internal<NextT, T>>(std::move(onFulfill), std::move(onReject), sharedContext); \
    node->chainNext(nextNode, [=]() { \
      auto runnable = std::bind(&Details::PromiseNode<NextT>::run, nextNode); \
      sharedContext->scheduleToRun(std::move(runnable)); \
    }); \
    \
    Promise<NextT> nextPromise; \
    nextPromise._node = nextNode; \
    return nextPromise; }

#define CALL_NODE_IMP(method) \
    { if (!_node) { \
        throw std::logic_error("invalid promise"); \
      } \
      return _node->method(); }

  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<T(void)>&& task, ThreadContext* &&context) 
    NEW_IMP(Details::PromiseNodeInternal)

#if DEFERRED_PROMISE
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context) 
    NEW_IMP(Details::DeferredPromiseNodeInternal)
#else
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<void(PromiseDefer<T>&&)>&&, ThreadContext* &&) {
    static_assert(falsehood<T>::value, "please enable `DEFERRED_PROMISE` in `PromiseConfig.h`");
  }
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<Promise<T>()>&& task, ThreadContext* &&context) 
    NEW_IMP(Details::NestingPromiseNodeInternal)
#else
  template<typename T>
  Promise<T> PromiseSpawner<T>::Spawn(std::function<Promise<T>()>&&, ThreadContext* &&) {
    static_assert(falsehood<T>::value, "please enable `NESTING_PROMISE` in `PromiseConfig.h`");
  }
#endif // NESTING_PROMISE

  template<typename T>
  template<typename NextT>
  Promise<NextT> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<NextT(T)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context) 
    THEN_IMP(Details::PromiseNodeInternal, T)

  template<typename NextT>
  Promise<NextT> PromiseThenable<void>::Then(SharedPromiseNode<void>& node,
                             std::function<NextT(void)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context)
    THEN_IMP(Details::PromiseNodeInternal, void)


#if DEFERRED_PROMISE
  template<typename T>
  template<typename NextT>
  Promise<NextT> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<void(PromiseDefer<NextT>&&, T)>&& onFulfill, 
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context)
    THEN_IMP(Details::DeferredPromiseNodeInternal, T)

  template<typename NextT>
  Promise<NextT> PromiseThenable<void>::Then(SharedPromiseNode<void>& node,
                             std::function<void(PromiseDefer<NextT>&&)>&& onFulfill, 
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context)
    THEN_IMP(Details::DeferredPromiseNodeInternal, void)
#else
  template<typename T>
  template<typename NextT>
  Promise<NextT> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
                             std::function<void(PromiseDefer<NextT>&&, T)>&& onFulfill, 
                             OnRejectFunction&& onReject, 
                             ThreadContext* &&context) {
    static_assert(falsehood<NextT>::value, "please enable `DEFERRED_PROMISE` in `PromiseConfig.h`");
  }

  template<typename NextT>
  Promise<NextT> PromiseThenable<void>::Then(SharedPromiseNode<void>& node,
                             std::function<void(PromiseDefer<NextT>&&)>&& onFulfill, 
                             OnRejectFunction&& onReject, 
                             ThreadContext* &&context) {
    static_assert(falsehood<NextT>::value, "please enable `DEFERRED_PROMISE` in `PromiseConfig.h`");
  }
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
  template<typename T>
  template<typename NextT>
  Promise<NextT> PromiseThenable<T>::Then(SharedPromiseNode<T>& node, 
                             std::function<Promise<NextT>(T)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context)
    THEN_IMP(Details::NestingPromiseNodeInternal, T)

  template<typename NextT>
  Promise<NextT> PromiseThenable<void>::Then(SharedPromiseNode<void>& node, 
                             std::function<Promise<NextT>(void)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context)
    THEN_IMP(Details::NestingPromiseNodeInternal, void)
#else
  template<typename T>
  template<typename NextT>
  Promise<NextT> PromiseThenable<T>::Then(SharedPromiseNode<T>& node, 
                             std::function<Promise<NextT>(T)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context) {
    static_assert(falsehood<NextT>::value, "please enable `NESTING_PROMISE` in `PromiseConfig.h`");
  }

  template<typename NextT>
  Promise<NextT> PromiseThenable<void>::Then(SharedPromiseNode<void>& node, 
                             std::function<Promise<NextT>(void)>&& onFulfill,
                             OnRejectFunction<NextT>&& onReject,
                             ThreadContext* &&context) {
    static_assert(falsehood<NextT>::value, "please enable `NESTING_PROMISE` in `PromiseConfig.h`");
  }
#endif // NESTING_PROMISE

  template<typename T> bool Promise<T>::isFulfilled() const CALL_NODE_IMP(isFulfilled)
  template<typename T> bool Promise<T>::isRejected() const CALL_NODE_IMP(isRejected)

  bool Promise<void>::isFulfilled() const CALL_NODE_IMP(isFulfilled)
  bool Promise<void>::isRejected() const CALL_NODE_IMP(isRejected)

  template<typename T>
  template<typename ArgType>
  Promise<T> Promise<T>::Resolved(ArgType&& arg) {
    Promise<T> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<T>>(std::forward<ArgType>(arg));
    spawned._node = node;

    return spawned;
  }

  template<typename T>
  Promise<T> Promise<T>::Rejected(std::exception_ptr e) {
    Promise<T> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<T>>(e);
    spawned._node = node;

    return spawned;
  }

  Promise<void> Promise<void>::Resolved() {
    Promise<void> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<void>>();
    spawned._node = node;

    return spawned;
  }

  Promise<void> Promise<void>::Rejected(std::exception_ptr e) {
    Promise<void> spawned;
    auto node = std::make_shared<Details::ResolvedRejectedPromiseInternals<void>>(e);
    spawned._node = node;

    return spawned;
  }
} // Promise2

#endif // PROMISE_PUBLIC_AP_ISIMPL_H
