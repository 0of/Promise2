/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef NESTING_PROMISE_INTERNALS_H
#define NESTING_PROMISE_INTERNALS_H

#include "../public/PromisePublicAPIs.h"
#include "PromiseInternalsBase.h"

namespace Promise2 {
	namespace Details {
		// nesting promise PromiseNodeInternal
    template<typename ReturnType, typename ArgType, typename ConvertibleArgType, typename IsTask = std::false_type>
    class NestingPromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType, IsTask> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType, IsTask>;
      using OnFulfill = std::function<Promise<UnboxVoid<ReturnType>>(ArgType)>;

    private:
      OnFulfill _onFulfill;

    public:
      NestingPromiseNodeInternal(OnFulfill&& onFulfill, 
                  OnRejectFunction<ReturnType>&& onReject,
                  const std::shared_ptr<ThreadContext>& context)
        : Base(std::move(onReject), context)
        , _onFulfill{ std::move(onFulfill) }
      {}

    protected:
      virtual void onRun() noexcept override {
        try {
          auto wrapped = std::move(_onFulfill(Base::template get<ConvertibleArgType>()));
          wrapped.internal()->chainNext(Base::_forward);
        } catch (...) {
          Base::_forward->reject(std::current_exception());
        }
      }

      virtual void onRun(Fulfillment<ArgType, IsTask>& fulfillment) noexcept override {
        try {
          auto wrapped = std::move(_onFulfill(fulfillment.template get<ConvertibleArgType>()));
          wrapped.internal()->chainNext(Base::_forward);
        } catch (...) {
          Base::_forward->reject(std::current_exception());
        }
      }
    };  
	} // Details
}

#endif // NESTING_PROMISE_INTERNALS_H
