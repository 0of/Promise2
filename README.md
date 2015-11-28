# Promise2
c++14 compliant cross-platform implementations of promise

# Features
- written with `future<T>` & `promise<F>`
- platform customized message deliver
- similar to [Promise](https://github.com/0of/Promise)
- no need to invoke the chain by method `Done()`

# API References
## Promise\<T\>
- Promise(std::function<T(void)>&& task, const ThreadContext& context)
> construct a promise with task and thread context(which thread is running at)

- template\<typename NextT\> Promise\<NextT\> then(std::function\<NextT(T)\>&& onFulfill, std::function<void(std::exception_ptr)>&& onReject, const ThreadContext& context) 
> chainify current promise to the next one

# Usage Guidelines
