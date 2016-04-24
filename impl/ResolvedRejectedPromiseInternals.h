/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef RESOLVED_REJECTED_PROMISE_INTERNALS_H
#define RESOLVED_REJECTED_PROMISE_INTERNALS_H
 
#include "PromiseInternalsBase.h"

namespace Promise2 {
  namespace Details {
    //
    // Resolved/Rejected promise internals
    //
    template<typename ReturnType>
    class ResolvedRejectedPromiseInternals : public PromiseNode<ReturnType> {
    private:
      SharedPromiseValue<ReturnType> _promiseValue;

    public:
      template<typename ValueType>
      explicit ResolvedRejectedPromiseInternals(ValueType&& v)
        : _promiseValue{ std::make_shared<typename SharedPromiseValue<ReturnType>::element_type>() } {
        // fulfilled the value
        ForwardFulfillPolicy<ReturnType>::wrappedFulfill(_promiseValue, std::forward<ValueType>(v));
      }

      // `void`
      ResolvedRejectedPromiseInternals()
        : _promiseValue{ std::make_shared<typename SharedPromiseValue<ReturnType>::element_type>() } {
        // fulfilled the value
        ForwardFulfillPolicy<ReturnType>::wrappedFulfill(_promiseValue, Void{});
      }

      // exception
      explicit ResolvedRejectedPromiseInternals(std::exception_ptr e)
        : _promiseValue{ std::make_shared<typename SharedPromiseValue<ReturnType>::element_type>() } {
        // rejected
        _promiseValue->setException(e);
      }

    public:
      virtual void run() override {}

      virtual void chainNext(const SharedNonTaskFulfill<ReturnType>& fulfill, std::function<void()>&& notify) override {
        fulfill->attach(_promiseValue);

        // promise value has been fulfilled or rejected
        notify();
      }

      virtual void chainNext(const DeferPromiseCore<ReturnType>& nextForward) override {
        if (_promiseValue->isExceptionCase()) {
          nextForward->reject(_promiseValue->fetchException());
        } else {
          ProxyForwardPolicy<ReturnType>::forwarding(nextForward, _promiseValue);
        }
      }

    public:
      virtual bool isFulfilled() const override {
        return _promiseValue->hasAssigned() && !_promiseValue->isExceptionCase();
      }

      virtual bool isRejected() const override {
        return _promiseValue->hasAssigned() && _promiseValue->isExceptionCase();
      }
    }; 
  } // Details
}
 
#endif // RESOLVED_REJECTED_PROMISE_INTERNALS_H
 