/*
* Promise2
*
* Copyright (c) 2015-2016 "0of" Magnus
* Licensed under the MIT license.
* https://github.com/0of/Promise2/blob/master/LICENSE
*/

#include "../public/PromisePublicAPIs.h"
//
// all `void` convert to `Void` for internal use
//
namespace Promise2 {
	namespace VoidTrait {
		
	  template<typename ReturnType, typename ArgType>
	  struct VoidTypeWrapper {
	    static auto currying(std::function<ReturnType(ArgType)>&& f) {
	      return std::move(f);
	    }
	  };

	  template<typename ArgType>
	  struct VoidTypeWrapper<void, ArgType> {
	    static auto currying(std::function<void(ArgType)>&& f) {
	      auto fn = [rawFn = std::move(f)](ArgType arg) {
	        rawFn(std::forward<ArgType>(arg));
	        return Void{};
	      };

	      return std::function<Void(ArgType)>{ std::move(fn) };
	    }
	  };

	  template<typename ReturnType>
	  struct VoidTypeWrapper<ReturnType, void> {
	    static auto currying(std::function<ReturnType(void)>&& f) {
	      auto fn = [rawFn = std::move(f)](Void) {
	        return std::forward<ReturnType>(rawFn());
	      };

	      return std::function<ReturnType(Void)>{ std::move(fn) };
	    }
	  };

	  template<>
	  struct VoidTypeWrapper<void, void> {
	    static auto currying(std::function<void(void)>&& f) {
	      auto fn = [rawFn = std::move(f)](Void) {
	        rawFn();
	        return Void{};
	      };

	      return std::function<Void(Void)>{ std::move(fn) };
	    }
	  };
	} // VoidTrait
}