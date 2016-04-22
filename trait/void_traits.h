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
		
		// return type
		template<typename ReturnType>
		struct ReturnVoid {
			template<typename... Args>
			static auto currying(std::function<ReturnType(Args...)>&& f) {
				return std::move(f);
			}
		};

		template<>
		struct ReturnVoid<void> {
			template<typename... Args>
			static auto currying(std::function<ReturnType(Args...)>&& f) {
				auto fn = [rawFn = std::move(f)](Args... args) {
	        rawFn(std::forward<ArgType...>(args...));
	        return Void{};
	      };

	      return std::function<Void(Args...)>{ std::move(fn) };
			}
		};

		// args type
		template<typename... Types>
		struct LastType {
			using Type = typename LastType<Types...>::Type;
		};

		template<typename T>
		struct LastType<T> {
			using Type = T;
		};

		template<typename T>
		struct LastTypeOfFunction {};

		template<typename ReturnType, typename Args...>
		struct LastTypeOfFunction<std::function<ReturnType(Args...)>> {
			using Type = typename LastType<Args...>::Type;
		};

		template<typename ReturnType>
		struct LastTypeOfFunction<std::function<ReturnType(void)>> {
			using Type = void;
		};

		template<typename Fn, typename T>
		struct LastArgTypeIs : public std::is_same<LastTypeOfFunction<Fn>::Type, T> {};

		template<typename ArgType>
		struct ArgVoid {
			template<typename ReturnType, typename... Args>
			static auto currying(std::function<ReturnType(Args...)>&& f) {
				return std::move(f);
			}
		};

		template<>
		struct ArgVoid<void> {
			// return type can not be void
			template<typename ReturnType, typename... Args>
			static auto currying(std::function<ReturnType(Args...)>&& f) {
 				auto fn = [rawFn = std::move(f)](Args... args, Void) {
	        return std::forward<ReturnType>(rawFn(args...));
	      };

				return std::function<ReturnType(Args..., Void)>{ std::move(fn) };
			}
		};
	} // VoidTrait
}