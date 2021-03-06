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
        _promiseValue->setValue(std::forward<ValueType>(v));
      }

      // exception
      explicit ResolvedRejectedPromiseInternals(std::exception_ptr e)
        : _promiseValue{ std::make_shared<typename SharedPromiseValue<ReturnType>::element_type>() } {
        // rejected
        _promiseValue->setException(e);
      }

    public:
      void runWith(const SharedPromiseValue<Void>&) {}

      void start() {}

      virtual void chainNext(const DeferPromiseCore<ReturnType>& nextForward) override {
        if (_promiseValue->isExceptionCase()) {
          nextForward->reject(_promiseValue->fetchException());
        } else {
          nextForward->fulfill(_promiseValue->template getValue<ReturnType>());
        }
      }

      virtual void chainNext(std::function<void(const SharedPromiseValue<ReturnType>&)>&& notify) override {
        // directly notify the receiver
        notify(_promiseValue);
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
 