# Promise2
[![Build Status](https://travis-ci.org/0of/Promise2.svg?branch=master)](https://travis-ci.org/0of/Promise2)
[![Build status](https://ci.appveyor.com/api/projects/status/0ipwbe082lmsi4p5?svg=true)](https://ci.appveyor.com/project/0of/promise2-un46g)
[![Circle CI](https://circleci.com/gh/0of/Promise2.svg?style=svg)](https://circleci.com/gh/0of/Promise2)
[![Coverage Status](https://coveralls.io/repos/github/0of/Promise2/badge.svg?branch=master)](https://coveralls.io/github/0of/Promise2?branch=master)

c++14 compliant cross-platform implementations of promise

[中文文档](https://github.com/0of/Promise2/blob/master/doc/zh-CN.md)

## Core concept
`Promise` provides a kind of mechanism to acquire the result which is type of the given `T` (voidness with `T == void`) or throw an exception later asynchronously. And each `Promise` should be bound to a `ThreadContext`, which means fulfill or reject the promise within specific context. `ThreadContext` will schedule either `fulfill` or `reject` both methods are passing to `Promise` to run asynchronously or synchronously under different situations.

`ThreadContext` represents the state of the running or pre-running thread and it has the ability to instruct the thread to schedule the task

> I recommend you to get familiar with [`Javascript Promise`](https://www.promisejs.org) though two totally different implementations and usages.

# Features
- simple API definitions and quite easy to use
- great extensibility and platform customized message deliver delegate
- lightweight
- all written in header files and easy to integrate

# Installation
### cmake
add the following code to your `CMakeLists.txt` file

_you may need to do some modifications for checking out specific release version_

```cmake
include(ExternalProject)
find_package(Git REQUIRED)

# checkout the source code from github
ExternalProject_Add(
    Promise2
    PREFIX "deps/root/path"
    GIT_REPOSITORY https://github.com/0of/Promise2.git
    TIMEOUT 10
    UPDATE_COMMAND ${GIT_EXECUTABLE} pull
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
)

# include the promise path
ExternalProject_Get_Property(Promise2 source_dir)
include_directories(${source_dir})

# c++14
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
```

### Compilers support
- GCC 5
- Clang 3.8
- VS2015

# Concept
### OnFulfill
*OnFulfill* is kind of function that will be invoked when previous promise has been fulfilled with such signatures `Return(Argument)` , and `void(PromiseDefer<Return>, Argument)` or
`Promise<Return>(Argument)` for some async operations. 

It is allowed to pass _static method_, _lambda expression_, _bind expression_ as function body.

> The `Return` type must be same type as `OnReject` returned

### OnReject
*OnReject* is much similar to `OnFulfill` but *OnReject* is invoked when previous promise has been rejected(exception thrown).

The signature of *OnReject* is `Promise<Return>(std::exception_ptr)` while `RecursionPromise<Return>(std::exception_ptr)` when involved with `RecursionPromise`

> The `Return` type must be same type as `OnFulfill` returned.
> Besides, you can just reject the return promise with the given argument `std::exception_ptr` or wrap a new one.

### Synchronous promise
The promise is resolved by an instance of the given type `T` or rejected by an exception as soon as the `fulfill` or `reject` [callables](http://en.cppreference.com/w/cpp/concept/Callable) returned.

### Recursion promise
Recursion promise is kind of promise that iterates a loop inside the target context and every chained thenable(NOT chained by `final()`) will
be fulfilled or rejected the same times as the iterator does. 

However if exception occurred during iteration, the recursion is halted and notify the `onReject` chained by `final()`.

Recursion Promise is designed for *event polling* or *channel consumer* and you just need to implement the *InputIterator* [concept](http://en.cppreference.com/w/cpp/concept/InputIterator)
and instantiate a _RecursionPromise_ with `begin` and `end` iterators.

Currently these operator implementations are required:
- `pre-increment`
- `deref`
- `not-equal`

### Deferable promise
The resolved state of the promise is not determinate by the time the `fulfill` [callables](http://en.cppreference.com/w/cpp/concept/Callable) returns. And it allows you to warp any asynchronous scatter operations into highly dense and clearly expressed code blocks.
```c++
// wrapped class for shipping the PromiseDefer object
class Context {
public:
  PromiseDefer<std::string> deferred;
};

// called when file read successfully
void OnReadFile(const std::string& content, Context *context) {
  context->deferred.setResult(content);
  // clean the context
  delete context;
}

Promise<std::string>::New([[](PromiseDefer<std::string>&& defer){
  Context *context = new Context;
  // move the defer object
  context->deferred = std::move(defer);

  ReadFile("package.json", OnReadFile, context /* passing the context */);
}, someContext);
```

### Nesting promise
The nesting promise, returned from given `fulfill` callable, resolves or rejects the outter one.

# Usage Guidelines
## Tips when you implement your own `ThreadContext`
- **When promise is fulfilled or rejected, the associated `ThreadContext` will be destroyed automatically**
- **Do NOT ignore the task and you should invoke it somewhere in your code once otherwise the promise chain will stop to propagate its state**

## Create a new promise
```c++
using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

auto promise = Promise<void>::New([]{
  // do something in new thread
}, STLThreadContext::New());
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
}, CurrentThreadContext::New());
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
  }, ThreadPoolThreadContext::New());
  
}, ThreadPoolThreadContext::New());

```

## Append a thenable
```c++
using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

auto promise = Promise<void>::New([]{
  // do something in new thread
}, STLThreadContext::New());

promise.then([]{
  // do something in new thread
}, [](std::exception_ptr) {
  // handle the exception
}, STLThreadContext::New());
```

## Append a deferred thenable
```c++
// libdispatch
using CurrentThreadContext = ThreadContextImpl::GCD::CurrentThreadContext;
using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

auto promise = Promise<void>::New([]{
  // do something in new thread
}, STLThreadContext::New());

promise.then([](PromiseDefer<int>&& defer){
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
}, STLThreadContext::New()).then([](int code){
  std::cout << code;
}, nullptr, CurrentThreadContext::New());
```

## Append a nesting thenable
```c++
using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;
// Win32
using ThreadPoolThreadContext = ThreadContextImpl::Windows::ThreadPoolContext; 

auto promise = Promise<void>::New([]{
  // do something in new thread
}, STLThreadContext::New());

promise.then([]() -> Promise<int> /* return a promise */ {
  // do something in thread pool
  return Promise<int>::New([]{
    // do some caculations
    return 1024;
  }, ThreadPoolThreadContext::New());
  
}, ThreadPoolThreadContext::New()).then([](int calResult){
  std::cout << calResult;
}, nullptr, STLThreadContext::New());
```

## Resolved & Rejected
```c++
using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

Promise<int>::Resolved(5).then([](int five) {
  std::cout << five;
}, STLThreadContext::New());

try {
  throw std::logic_error("logic error?");
} catch (...) {
  Promise<int>::Rejected(std::current_exception()).then([](int) {
    std::cout << "Never called!";
  }, [](std::exception_ptr e) {
    std::cout << "something's wrong?!";
  }, STLThreadContext::New());
}
```

## Recursion Promise
```c++
class EventPollIterator {
private:
  ConsumerChan chan;

public:
  EventPollIterator() = default;
  template<typename String>
  EventPollIterator(String&& opts) 
    :chan { ConsumerChan::New(std::forward<String>(opts)) }
  {}

  bool operator != (const EventPollIterator& i) { return i.chan != chan; }

  std::string operator *() {
    return chan.value();
  }

  EventPollIterator& operator++ () {
    if (chan.will_quit()) {
      chan.reset();
    } else {
      // block to poll new event
      chan.poll();
    }

    return *this;
  }
};

using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

Promise2::RecursionPromise<std::string>::Iterate(EventPollIterator("id"), EventPollIterator(), new STLThreadContext()).
    then([=](std::string ev) { std::cout << ev; }, 
          [=](std::exception_ptr ) { return Promise2::RecursionPromise<void>(); },
          new UserContext()).
    final([=]() { std::cout << "chan finished!" }
          [=](std::exception_ptr e) { return Promise2::Promise<void>::Rejected(e); },
          new context());
```

## Working with Objective-c++
- enable ARC(**add compiler option `-fobjc-arc` if not using Xcode**)
- use `.mm` as your implement file extension
- decorate `__block` keyword to c++ instance when using with objc block

```objective-c++
Promise2::Promise<BOOL>::Resolved(YES).then(^(BOOL fulfilled) { 
  if (YES != fulfilled) { 
    // assert?
    return; 
  }
}, ^(std::exception_ptr e) {
  return Promise2::Promise<void>::Rejected(e);
}, MainThreadContext::New());
```

## Default implemented `ThreadContext`
- GCD thread context
- Win32 thread context
- STL thread context

# TODOs
- test cases
- promise extensions

# License
  MIT License



