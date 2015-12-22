/*
 * Promise2
 *
 * Copyright (c) 2015 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#include "ThreadContext_GCD.h"

/**
 * disable objc
 */
#ifndef OS_OBJECT_USE_OBJC
#define OS_OBJECT_USE_OBJC 0
#endif

#include <dispatch/dispatch.h>

namespace ThreadContextImpl {
	namespace GCD {
		using Task = std::function<void()>;

		static void InvokeFunction(void *context) {
			std::unique_ptr<Task> function{ std::static_cast<Task *>(context) };
			(*function)();
		}

		ThreadContext *QueueBasedThreadContext::New(dispatch_queue_t queue) {
			QueueBasedThreadContext *context = new QueueBasedThreadContext;
			if (context) {
				context->_queue = queue;
			}
			
			return context;
		}

		void QueueBasedThreadContext::scheduleToRun(std::function<void()>&& task) {
			dispatch_async_f(_queue, new Task{ std::move(task) }, InvokeFunction);
		}

		ThreadContext *CurrentThreadContext::New() {
			return QueueBasedThreadContext::New(dispatch_get_current_queue());
		}

		ThreadContext *MainThreadContext::New() {
			return QueueBasedThreadContext::New(dispatch_get_main_queue());
		}
	}
}





