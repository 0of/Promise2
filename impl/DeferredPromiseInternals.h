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
  PromiseDefer<T>::PromiseDefer(Details::DeferPromiseCore<T>& core)
    : _core{ core }
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

  PromiseDefer<void>::PromiseDefer(Details::DeferPromiseCore<void>& core)
    : _core{ core }
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
    template<typename ReturnType, typename ArgType, typename ConvertibleArgType, typename IsTask = std::false_type>
    class DeferredPromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType, std::false_type> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType, std::false_type>;

    private:
      std::function<void(PromiseDefer<ReturnType>&&, ConvertibleArgType)> _onFulfill;

    public:
      DeferredPromiseNodeInternal(std::function<void(PromiseDefer<ReturnType>&&, ConvertibleArgType)>&& onFulfill,
                  OnRejectFunction<ReturnType>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _onFulfill{ std::move(onFulfill) }
      {}

    public:
      virtual void run() noexcept override {
        std::call_once(Base::_called, [this]() {
          ArgType preValue;

          try {
            Base::guard();
          } catch (...) {
            Base::runReject();
            return;
          }

          PromiseDefer<ReturnType> deferred{ Base::_forward };
          // no exception allowed
          _onFulfill(std::move(deferred), std::forward<ConvertibleArgType>(Base::_previousPromise->value));
        });
      }
    };

    template<typename ReturnType, typename IsTask>
    class DeferredPromiseNodeInternal<ReturnType, void, void, IsTask> : public PromiseNodeInternalBase<ReturnType, void, IsTask> {
      using Base = PromiseNodeInternalBase<ReturnType, void, IsTask>;

    private:
      std::function<void(PromiseDefer<ReturnType>&&)> _onFulfill;

    public:
      DeferredPromiseNodeInternal(std::function<void(PromiseDefer<ReturnType>&&)>&& onFulfill, 
                  OnRejectFunction<ReturnType>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _onFulfill{ std::move(onFulfill) }
      {}

    public:
      virtual void run() noexcept override {
        std::call_once(Base::_called, [this]() {
          try {
            Base::guard();
          } catch (...) {
            Base::runReject();
            return;
          }

          PromiseDefer<ReturnType> deferred{ Base::_forward };
          // no exception allowed
          _onFulfill(std::move(deferred));
        });
      }
    };
  }
}

#endif // DEFERRED_PROMISE_INTERNALS_H
