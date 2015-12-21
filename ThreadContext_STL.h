/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef THREAD_CONTEXT_STL_H
#define THREAD_CONTEXT_STL_H

#include <thread>

#include "PromisePublicAPIs"

namespace ThreadContextImpl {
	namespace STL {
		class DetachedThreadContext : public ThreadContext {
		public:
			static ThreadContext *New();

		private:
			std::thread _detachedThread;

		protected:
			DetachedThreadContext() = default;

		public:
			virtual ~DetachedThreadContext() = default;

		public:
			virtual void scheduleToRun(std::function<void()>&& task) override;

		private:
			DetachedThreadContext(const DetachedThreadContext& ) = delete;
			DetachedThreadContext& operator = (const DetachedThreadContext& ) = delete;
		};
	} // STL
}	

#endif // THREAD_CONTEXT_STL_H
