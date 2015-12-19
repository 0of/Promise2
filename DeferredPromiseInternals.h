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
  // @class PromiseDefer
  //
  template<typename T>
  PromiseDefer<T>::PromiseDefer(Details::DeferPromiseCore<T>&& core)
    : _core{ std::move(core) }
  {}

  template<typename T>
  template<typename X>
  void PromiseDefer<T>::setResult(X&& r) {
    _core->fulfill(std::forward<X>(r));
  }

  template<typename T>
  void PromiseDefer<T>::setException(std::exception_ptr e) {
    _core->reject(e);
  }

  PromiseDefer<void>::PromiseDefer(Details::DeferPromiseCore<void>&& core)
    : _core{ std::move(core) }
  {}

	void PromiseDefer<void>::setResult() {
		_core->fulfill();
	}

  void PromiseDefer<void>::setException(std::exception_ptr e) {
  	_core->reject(e);
  }
}

namespace Promise2 {
  namespace Details {
    // deferred PromiseNodeInternal
    template<typename ReturnType, typename ArgType>
    class DeferredPromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType>;

    private:
      std::function<void(PromiseDefer<ReturnType>&&, ArgType)> _onFulfill;

    public:
      DeferredPromiseNodeInternal(std::function<void(PromiseDefer<ReturnType>&&, ArgType)>& onFulfill, 
                  std::function<void(std::exception_ptr)>&& onReject,
                  std::shared_ptr<ThreadContext>&& context)
        : PromiseNodeInternalBase<ReturnType, ArgType>{ std::move(onReject), std::move(context) }
        , _onFulfill{ std::move(onFulfill) }
      {}

    public:
      virtual void run() override {
        std::call_once(Base::_called, [&]() {
          ArgType preValue;

          try {
            preValue = Fulfill<ArgType>::get();
          } catch (...) {
            Base::runReject();
            return;
          }

          try {
            PromiseDefer<ReturnType> deferred{ std::move(Base::_forward) };
            _onFulfill(deferred, preValue);
          } catch (...) {
            // previous task is failed
            Base::runReject();
          }
        });
      }
    };
  }
}

#endif // DEFERRED_PROMISE_INTERNALS_H
