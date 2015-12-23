/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef THREAD_CONTEXT_WINDOWS_H
#define THREAD_CONTEXT_WINDOWS_H

#if defined(_WIN32) || defined(_WIN64)
#include "PromisePublicAPIs"

namespace ThreadContextImpl {
	namespace Windows {
		class ThreadPoolContext : public ThreadContext {
		public:
			static ThreadContext *New();

		protected:
			ThreadPoolContext() = default;

		public:
			virtual ~ThreadPoolContext() = default;

		public:
			virtual void scheduleToRun(std::function<void()>&& task) override;

		private:
			ThreadPoolContext(const ThreadPoolContext& ) = delete;
			ThreadPoolContext& operator = (const ThreadPoolContext& ) = delete;
		};
	} // Windows
}	

#endif // windows

#endif // THREAD_CONTEXT_WINDOWS_H
