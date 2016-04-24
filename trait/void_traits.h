/*
* Promise2
*
* Copyright (c) 2015-2016 "0of" Magnus
* Licensed under the MIT license.
* https://github.com/0of/Promise2/blob/master/LICENSE
*/

#ifndef VOID_TRAITS_H
#define VOID_TRAITS_H

#include "../public/PromisePublicAPIs.h"
//
// all `void` convert to `Void` for internal use
//
namespace Promise2 {
	namespace VoidTrait {

		// return type
		template<typename ReturnType>
		struct ReturnVoid {
			template<typename... Args>
			static auto currying(std::function<ReturnType(Args...)>&& f) {
				return std::move(f);
			}

			static auto currying(std::function<ReturnType()>&& f) {
				return std::move(f);
			}
		};

		template<>
		struct ReturnVoid<void> {
			template<typename... Args>
			static auto currying(std::function<void(Args...)>&& f) {
				auto fn = [rawFn = std::move(f)](Args... args) {
	        rawFn(std::forward<Args...>(args...));
	        return Void{};
	      };

	      return std::function<Void(Args...)>{ std::move(fn) };
			}

			static auto currying(std::function<void()>&& f) {
				auto fn = [rawFn = std::move(f)]() {
	        rawFn();
	        return Void{};
	      };

	      return std::function<Void()>{ std::move(fn) };
			}
		};

		// args type
		template<typename T, typename... Types>
		struct LastType {
			using Type = typename LastType<Types...>::Type;
		};

		template<typename T>
		struct LastType<T> {
			using Type = T;
		};

		template<typename ArgType>
		struct ArgVoid {
			template<typename ReturnType, typename... Args>
			static auto currying(std::function<ReturnType(Args...)>&& f) {
				return std::move(f);
			}
		};
        
        template<typename... T>
        using last_of = typename LastType<T...>::Type;

		template<>
		struct ArgVoid<void> {
			// return type can not be void
			template<typename ReturnType, typename... Args>
			static auto currying(std::function<ReturnType(Args...)>&& f) {
 				auto fn = [rawFn = std::move(f)](Args... args, Void) {
          return rawFn(std::forward<Args...>(args...));
	      };

				return std::function<ReturnType(Args..., Void)>{ std::move(fn) };
			}

			template<typename ReturnType>
			static auto currying(std::function<ReturnType()>&& f) {
 				auto fn = [rawFn = std::move(f)](Void) {
          return rawFn();
	      };

				return std::function<ReturnType(Void)>{ std::move(fn) };
			}
		};
	} // VoidTrait
}

#endif // VOID_TRAITS_H