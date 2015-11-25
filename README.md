# Promise2
c++14 compliant cross-platform implementations of promise

# Features
- written with `future<T>` & `promise<F>`
- platform customized message deliver
- similar to [Promise](https://github.com/0of/Promise)
- no need to invoke the chain by method `Done()`

# API References
## Promise\<T\>
- Promise(std::functional\<T()\>&& task, ThreadContext context)
> construct a promise with task and thread context(which thread is running at)

- Promise\<Return\> then(std::functional\<Return(T)\>&& chainedTask, ThreadContext context)
> chainify current promise to the next one

# Usage Guidelines
