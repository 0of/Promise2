/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef NESTING_PROMISE_INTERNALS_H
#define NESTING_PROMISE_INTERNALS_H

#include "PromiseInternalsBase.h"

namespace Promise2 {
	namespace Details {
		// nesting promise PromiseNodeInternal
    template<typename ReturnType, typename ArgType>
    class NestingPromiseNodeInternal : public PromiseNodeInternalBase<ReturnType, ArgType> {
      using Base = PromiseNodeInternalBase<ReturnType, ArgType>;

    private:
      std::function<Promise<ReturnType>(ArgType)> _onFulfill;

    public:
      NestingPromiseNodeInternal(std::function<Promise<ReturnType>(ArgType)>& onFulfill, 
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

            // return a promise will hold the return value
            _onFulfill(preValue).then([&](ReturnType v){
              deferred.setResult(v);
            });

          } catch (...) {
            // previous task is failed
            Base::runReject();
          }
      	});
      }
    };  
	} // Details
}

#endif // NESTING_PROMISE_INTERNALS_H
