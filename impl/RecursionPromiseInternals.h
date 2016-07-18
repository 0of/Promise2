/*
 * Promise2
 *
 * Copyright (c) 2016 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef RECURSION_PROMISE_INTERNALS_H
#define RECURSION_PROMISE_INTERNALS_H

#include "../public/PromisePublicAPIs.h"
#include "PromiseInternalsBase.h"

namespace Promise2 {
  namespace Details {

    template<typename ReturnType, 
             typename ArgType, 
             typename ConvertibleArgType,
             typename IsTask,
             typename InputIterator>
    class RecursionPromiseNodeInternal : public RecursionPromiseNodeInternalBase<ReturnType, ArgType, IsTask> {
      using Base = RecursionPromiseNodeInternalBase<ReturnType, ArgType, IsTask>;

    private:
      InputIterator _iter;
      InputIterator _end;

    public:
      RecursionPromiseNodeInternal(const InputIterator& begin,
                                   const InputIterator& end,
                                   OnRejectFunction<ReturnType>&& onReject,
                                   const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _iter{ begin }
        , _end{ end }
      {}

    protected:
      virtual void onRun(Fulfillment<ArgType, IsTask>& fulfillment) noexcept override {
        try {
          for (; _iter != _end; ++_iter) {
            Base::_forward->fulfill(*_iter);
          }
        } catch (...) {
          Base::_finishForward->reject(std::current_exception());
          return;
        }

        Base::_finishForward->fulfill(Void{});
      }
    };
    
  }
}

#endif // RECURSION_PROMISE_INTERNALS_H
