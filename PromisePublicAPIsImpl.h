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

	template<typename T>
	Promise<T> PromiseSpawner<T>::New(std::function<T(void)>&& task, ThreadContext* &&context) {
		Promise<T> spawned;
	  auto node = std::make_shared<Details::PromiseNodeInternal<T, void>>(std::move(task), 
	                std::function<void(std::exception_ptr)>(), std::move(context));

	  context->scheduleToRun(&Details::PromiseNode<T>::run, node);

	  spawned._node = node;
	  return spawned;
	}

#if DEFERRED_PROMISE
		template<typename T>
	  Promise<T> PromiseSpawner<T>::New(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context) {
	  	Promise<T> spawned;
		  auto node = std::make_shared<Details::DeferredPromiseNodeInternal<T, void>>(std::move(task), 
		                std::function<void(std::exception_ptr)>(), std::move(context));

		  context->scheduleToRun(&Details::PromiseNode<T>::run, node);

		  spawned._node = node;
		  return spawned;
	  }
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
		template<typename T>
	  Promise<T> PromiseSpawner<T>::New(std::function<Promise<T>()>&& task, ThreadContext* &&context) {
	  	Promise<T> spawned;
		  auto node = std::make_shared<Details::NestingPromiseNodeInternal<T, void>>(std::move(task), 
		                std::function<void(std::exception_ptr)>(), std::move(context));

		  context->scheduleToRun(&Details::PromiseNode<T>::run, node);

		  spawned._node = node;
		  return spawned;
	  }
#endif // NESTING_PROMISE


	template<typename T>
	template<typename NextT, typename OnFulfill>
  Promise<NextT> PromiseThenable<T>::Then(SharedPromiseNode<T>& node,
  										       OnFulfill&& onFulfill, 
                             std::function<void(std::exception_ptr)>&& onReject, 
                             ThreadContext* &&context) {

    auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context));

    auto nextNode = std::make_shared<Details::PromiseNodeInternal<T, void>>(std::move(onFulfill), std::move(onReject), std::move(context));
    node->chainNext(nextNode, [&]() {
      sharedContext->scheduleToRun(&Details::PromiseNode<T>::run, nextNode);
    });

    // all OK, return the wrapped object
    Promise<NextT> nextPromise;
    nextPromise._node = nextNode;
    return nextPromise;
  }

#if DEFERRED_PROMISE
  	template<typename T>
    template<typename NextT, typename OnFulfill>
    Promise<NextT> PromiseThenable<T>::ThenDeferred(SharedPromiseNode<T>& node,
                               OnFulfill&& onFulfill, 
                               std::function<void(std::exception_ptr)>&& onReject, 
                               ThreadContext* &&context) {
    	auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context));

	    auto nextNode = std::make_shared<Details::DeferredPromiseNodeInternal<T, void>>(std::move(onFulfill), std::move(onReject), std::move(context));
	    node->chainNext(nextNode, [&]() {
	      sharedContext->scheduleToRun(&Details::PromiseNode<T>::run, nextNode);
	    });

	    // all OK, return the wrapped object
	    Promise<NextT> nextPromise;
	    nextPromise._node = nextNode;
	    return nextPromise;
    }
#endif // DEFERRED_PROMISE

#if NESTING_PROMISE
    template<typename T>
    template<typename NextT, typename OnFulfill>
    Promise<NextT> PromiseThenable<T>::ThenNesting(SharedPromiseNode<T>& node, 
    	                         OnFulfill&& onFulfill,
                               std::function<void(std::exception_ptr)>&& onReject, 
                               ThreadContext* &&context) {
    	auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context));

	    auto nextNode = std::make_shared<Details::NestingPromiseNodeInternal<NextT, T>>(std::move(onFulfill), std::move(onReject), std::move(context));
	    node->chainNext(nextNode, [&]() {
	      sharedContext->scheduleToRun(&Details::PromiseNode<T>::run, nextNode);
	    });

	    // all OK, return the wrapped object
	    Promise<NextT> nextPromise;
	    nextPromise._node = nextNode;
	    return nextPromise;
    }
#endif // NESTING_PROMISE
} // Promise2

#endif // PROMISE_PUBLIC_AP_ISIMPL_H
