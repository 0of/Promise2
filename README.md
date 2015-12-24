# Promise2
c++14 compliant cross-platform implementations of promise

# Features
- written with `future<T>` & `promise<F>`
- platform customized message deliver
- similar to [Promise](https://github.com/0of/Promise)
- no need to invoke the chain by method `Done()`

# Usage Guidelines
## Create a new promise
```c++
using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

auto promise = Promise<void>::New([]{
  // do something in new thread
}, new STLThreadContext);
```

## Create a deferred promise
```c++
// libdispatch
using CurrentThreadContext = ThreadContextImpl::GCD::CurrentThreadContext;

auto promise = Promise<int>::New([](PromiseDefer<int>&& defer){
  // do something in current thread
  doSomeAsync([deferred = std::move(defer)] (int code) mutable {
    // when finshed
    if (code < 0) {
        // something terrible happens
        try {
            convertCodeToException(code);
        } catch (...) {
            deferred.setException(std::current_exception());
            return;
        }
    }
    
    deferred.setResult(code);
  })
}, new STLThreadContext);
```

## Create a nesting promise
```c++
// Win32
using ThreadPoolThreadContext = ThreadContextImpl::Windows::ThreadPoolContext; 

// current promise will forward the result till the nesting one finished
auto promise = Promise<int>::New([]() -> Promise<int> /* return a promise */ {
  // do something in thread pool
  return Promise<int>::New([]{
    // do some caculations
    return 1024;
  }, new ThreadPoolThreadContext);
  
}, new ThreadPoolThreadContext);

```
