/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef DEFERRED_PROMISE_INTERNALS_H
#define DEFERRED_PROMISE_INTERNALS_H

#include "PromiseInternalsBase.h"

namespace Promise2 {
  //
  // @class PromiseDeferBase
  //
  template<typename T, typename RecursionMode>
  PromiseDeferBase<T, RecursionMode>::PromiseDeferBase(DeferCoreType<T, RecursionMode>& core)
    : _core{ core }
  {}

  template<typename T, typename RecursionMode>
  template<typename X>
  void PromiseDeferBase<T, RecursionMode>::setResult(X&& r) {
    _core->fulfill(std::forward<X>(r));
  }

  template<typename T, typename RecursionMode>
  void PromiseDeferBase<T, RecursionMode>::setException(std::exception_ptr e) {
    _core->reject(e);
  }

  template<typename RecursionMode>
  PromiseDeferBase<void, RecursionMode>::PromiseDeferBase(DeferCoreType<void, RecursionMode>& core)
    : _core{ core }
  {}

  template<typename RecursionMode>
	void PromiseDeferBase<void, RecursionMode>::setResult() {
    _core->fulfill(Void{});
	}

  template<typename RecursionMode>
  void PromiseDeferBase<void, RecursionMode>::setException(std::exception_ptr e) {
  	_core->reject(e);
  }
}

namespace Promise2 {
  namespace Details {
    // deferred PromiseNodeInternal
    template<typename ReturnType, typename ArgType, typename ConvertibleArgType, typename IsTask = std::false_type>
    class DeferredPromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType, IsTask> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType, IsTask>;
      using Defer = PromiseDefer<UnboxVoid<ReturnType>>;

    private:
      std::function<Void(Defer&&, ConvertibleArgType)> _onFulfill;

    public:
      DeferredPromiseNodeInternal(std::function<Void(Defer&&, ConvertibleArgType)>&& onFulfill,
                  OnRejectFunction<ReturnType>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _onFulfill{ std::move(onFulfill) }
      {}

    protected:
      virtual void onRun(Fulfillment<ArgType, IsTask>& fulfillment) noexcept override {
        try {
          Defer deferred{ Base::_forward };
          // no exception allowed
          _onFulfill(std::move(deferred), fulfillment.template get<ConvertibleArgType>());
        } catch (...) {
          Base::_forward->reject(std::current_exception());
        }
      }
    };
  }
}

#endif // DEFERRED_PROMISE_INTERNALS_H
