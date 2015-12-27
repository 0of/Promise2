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

namespace Promise2 {

/**
 * Implementations
 */
#define NEW_IMP(internal) \
	 { Promise<T> spawned; \
	 auto node = std::make_shared<internal<T, void>>(std::move(task), \
	                std::function<void(std::exception_ptr)>(), std::move(context)); \
	 context->scheduleToRun(&Details::PromiseNode<T>::run, node); \
	 \
	 spawned._node = node; \
	 return spawned; }

#define THEN_IMP(internal) \
	{ auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context)); \
		\
    auto nextNode = std::make_shared<internal<T, void>>(std::move(onFulfill), std::move(onReject), std::move(context)); \
    node->chainNext(nextNode, [&]() { \
      sharedContext->scheduleToRun(&Details::PromiseNode<T>::run, nextNode); \
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
	Promise<T> PromiseSpawner<T>::New(std::function<T(void)>&& task, ThreadContext* &&context) 
		NEW_IMP(Details::PromiseNodeInternal)

#if DEFERRED_PROMISE
	template<typename T>
  Promise<T> PromiseSpawner<T>::New(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context) 
  	NEW_IMP(Details::DeferredPromiseNodeInternal)
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
	template<typename T>
  Promise<T> PromiseSpawner<T>::New(std::function<Promise<T>()>&& task, ThreadContext* &&context) 
  	NEW_IMP(Details::NestingPromiseNodeInternal)
#endif // NESTING_PROMISE


	template<typename T>
	template<typename NextT, typename OnFulfill>
  Promise<NextT> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
  										       OnFulfill&& onFulfill, 
                             std::function<void(std::exception_ptr)>&& onReject, 
                             ThreadContext* &&context) 
    THEN_IMP(Details::PromiseNodeInternal)

#if DEFERRED_PROMISE
  	template<typename T>
    template<typename NextT, typename OnFulfill>
    Promise<NextT> PromiseThenable<T>::ThenDeferred(SharedPromiseNode<T>& node,
                               OnFulfill&& onFulfill, 
                               std::function<void(std::exception_ptr)>&& onReject, 
                               ThreadContext* &&context)
   	THEN_IMP(Details::DeferredPromiseNodeInternal) 
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
    template<typename T>
    template<typename NextT, typename OnFulfill>
    Promise<NextT> PromiseThenable<T>::ThenNesting(SharedPromiseNode<T>& node, 
    	                         OnFulfill&& onFulfill,
                               std::function<void(std::exception_ptr)>&& onReject, 
                               ThreadContext* &&context)
    THEN_IMP(Details::NestingPromiseNodeInternal)
#endif // NESTING_PROMISE

    template<typename T> bool Promise<T>::isFulfilled() const CALL_NODE_IMP(isFulfilled)
    template<typename T> bool Promise<T>::isRejected() const CALL_NODE_IMP(isRejected)

    bool Promise<void>::isFulfilled() const CALL_NODE_IMP(isFulfilled)
    bool Promise<void>::isRejected() const CALL_NODE_IMP(isRejected)
} // Promise2

#endif // PROMISE_PUBLIC_AP_ISIMPL_H
