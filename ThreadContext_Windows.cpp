#include "ThreadContext_Windows.h"

#if defined(_WIN32) || defined(_WIN64)
namespace ThreadContextImpl {
	namespace Windows {
		using Task = std::function<void()>;

		static void InvokeFunction(void *context) {
			std::unique_ptr<Task> function{ std::static_cast<Task *>(context) };
			(*function)();
		}

		ThreadContext *ThreadPoolContext::New() {
			return new ThreadPoolContext;
		}

		void ThreadPoolContext::scheduleToRun(std::function<void()>&& task) {
			 ::QueueUserWorkItem(InvokeFunction, new Task{ std::move(task) }, 0);
		}
	} // Windows
}	

#endif