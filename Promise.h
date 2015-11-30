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
    std::shared_ptr<Details::PromiseNode> _node; 

  public:
    // empty constructor
    Promise() = default;
    
    // constructor with task and running context
    Promise(std::function<T(void)>&& task, const ThreadContext& context)
      : _node() {
      _node = std::make_shared<Details::PromiseNodeInternal<T, void>>(std::move(task), 
                    std::function<void(std::exception_ptr)>(), context);

      // schedule to run
      _node->postToRun();
    }

  public:
    template<typename NextT>
    Promise<NextT> then(std::function<NextT(T)>&& onFulfill, 
                        std::function<void(std::exception_ptr)>&& onReject, 
                        const ThreadContext& context) {
      if (!isValid()) {
        throw std::logic_error("");
      }

      auto nextNode = std::make_shared<Details::PromiseNodeInternal<T, void>>(std::move(onFulfill), std::move(onReject), context);
      _node->chainNext(nextNode);

      // all OK, return the wrapped object
      Promise<NextT> nextPromise;
      nextPromise._node = nextNode;
      return nextPromise;
    }

  public:
    bool isValid() const {
      return !!_node;
    }
  };
}

#endif // PROMISE_H
