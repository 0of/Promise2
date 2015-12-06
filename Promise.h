
/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef PROMISE_H
#define PROMISE_H

#include "PromiseInternals.h"

namespace Promise2 {

  //
  // @class Promise
  //
  template<typename T>
  class Promise {
  private:
    std::shared_ptr<Details::PromiseNode<T>> _node; 

  public:
    // empty constructor
    Promise() = default;
    
    // constructor with task and running context
    Promise(std::function<T(void)>&& task, ThreadContext* &&context)
      : _node() {
      _node = std::make_shared<Details::PromiseNodeInternal<T, void>>(std::move(task), 
                    std::function<void(std::exception_ptr)>(), std::move(context));

      context->scheduleToRun(&Details::PromiseNode<T>::run, _node);
    }

    // constructor with deferred task
    Promise(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context)
      : _node() {
     
     _node = std::make_shared<Details::DeferredPromiseNodeInternal<T, void>>(std::move(task), 
                    std::function<void(std::exception_ptr)>(), std::move(context));

      context->scheduleToRun(&Details::PromiseNode<T>::run, _node);
    }

    Promise(Promise&& promise)
      : _node{ std::move(promise._node) }
    {}

    Promise& operator = (Promise&& promise) {
      _node = std::move(promise._node);
    }

  public:
    template<typename NextT>
    Promise<NextT> then(std::function<NextT(T)>&& onFulfill, 
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

    template<typename NextT>
    Promise<NextT> then(std::function<void(PromiseDefer<NextT>&&, T)>&& onFulfill,
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


  public:
    bool isValid() const {
      return !!_node;
    }

  private:
    Promise(const Promise& ) = delete;
    Promise& operator = (const Promise& ) = delete;
  };
}

#endif // PROMISE_H
