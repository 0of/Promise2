/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#include "ThreadContext_STL.h"

namespace ThreadContextImpl {
	namespace STL {
		ThreadContext *DetachedThreadContext::New() {
			return new DetachedThreadContext;
		}

		void DetachedThreadContext::scheduleToRun(std::function<void()>&& task) {
			std::thread taskThread{ std::move(task) };
			_detachedThread = std::move(taskThread);

			_detachedThread.detach();
		}
	}
}