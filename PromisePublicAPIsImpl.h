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
  Promise<T>::Promise(std::function<T(void)>&& task, ThreadContext* &&context)
      : _node() {
      _node = std::make_shared<Details::PromiseNodeInternal<T, void>>(std::move(task), 
                    std::function<void(std::exception_ptr)>(), std::move(context));

      context->scheduleToRun(&Details::PromiseNode<T>::run, _node);
  }

#if DEFERRED_PROMISE
  template<typename T>
  Promise<T>::Promise(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context)
    : _node() {
   
   _node = std::make_shared<Details::DeferredPromiseNodeInternal<T, void>>(std::move(task), 
                  std::function<void(std::exception_ptr)>(), std::move(context));

    context->scheduleToRun(&Details::PromiseNode<T>::run, _node);
  }
#endif // DEFERRED_PROMISE

  template<typename T>
  Promise<T>::Promise(Promise<T>&& promise)
    : _node{ std::move(promise._node) }
  {}

  template<typename T>
  Promise<T>& Promise<T>::operator = (Promise<T>&& promise) {
    _node = std::move(promise._node);
  }

  template<typename T>
  template<typename NextT>
  Promise<NextT> Promise<T>::then(std::function<NextT(T)>&& onFulfill, 
                      std::function<void(std::exception_ptr)>&& onReject, 
                      ThreadContext* &&context) {
    if (!isValid()) {
      throw std::logic_error("invalid promise");
    }

    auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context));

    auto nextNode = std::make_shared<Details::PromiseNodeInternal<T, void>>(std::move(onFulfill), std::move(onReject), std::move(context));
    _node->chainNext(nextNode, [&]() {
      sharedContext->scheduleToRun(&Details::PromiseNode<T>::run, nextNode);
    });

    // all OK, return the wrapped object
    Promise<NextT> nextPromise;
    nextPromise._node = nextNode;
    return nextPromise;
  }

#if DEFERRED_PROMISE
  template<typename T>
  template<typename NextT>
  Promise<NextT> Promise<T>::then(std::function<void(PromiseDefer<NextT>&&, T)>&& onFulfill,
                      std::function<void(std::exception_ptr)>&& onReject, 
                      ThreadContext* &&context) {
    if (!isValid()) {
      throw std::logic_error("invalid promise");
    }

    auto sharedContext = std::shared_ptr<ThreadContext>(std::move(context));

    auto nextNode = std::make_shared<Details::DeferredPromiseNodeInternal<T, void>>(std::move(onFulfill), std::move(onReject), std::move(context));
    _node->chainNext(nextNode, [&]() {
      sharedContext->scheduleToRun(&Details::PromiseNode<T>::run, nextNode);
    });

    // all OK, return the wrapped object
    Promise<NextT> nextPromise;
    nextPromise._node = nextNode;
    return nextPromise;
  }
#endif // DEFERRED_PROMISE
} // Promise2

#endif // PROMISE_PUBLIC_AP_ISIMPL_H
