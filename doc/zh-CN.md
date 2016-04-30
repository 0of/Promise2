# Promise2
[![Build Status](https://travis-ci.org/0of/Promise2.svg?branch=master)](https://travis-ci.org/0of/Promise2)
[![Build status](https://ci.appveyor.com/api/projects/status/0ipwbe082lmsi4p5?svg=true)](https://ci.appveyor.com/project/0of/promise2-un46g)
[![Circle CI](https://circleci.com/gh/0of/Promise2.svg?style=svg)](https://circleci.com/gh/0of/Promise2)
[![Coverage Status](https://coveralls.io/repos/github/0of/Promise2/badge.svg?branch=master)](https://coveralls.io/github/0of/Promise2?branch=master)

c++14 标准跨平台实现 `Promise`

# 特性
- 定义清晰，使用简单
- 易于适配，平台制定
- 精简轻量
- 无需配置，直接引用

# 安装
### cmake
添加如下至您的 `CMakeLists.txt` 中

_您可能需要做些修改来符合您的实际需求_

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

### 编译器版本支持
- GCC 5
- Clang 3.8
- VS2015

# 概念
## ThreadContext
`ThreadContext` 线程上下文是任务运行的环境，同时具备调度任务的能力

### 实现 `ThreadContext` 注意事项
- **当promise满足或拒绝后，关联的线程上下文就会被自动释放**
- **不要忽略调度的任务，该任务必须在某个时间点被执行，否则其后节点永远都不会被调度执行**

### 默认实现的 `ThreadContext`
- GCD thread context
- Win32 thread context
- STL thread context

## Promise\<T\>
`Promise` 表达的是一种满足承诺的语义，并提供了一种机制，能在异步任务处理结束后，传达对应的结果或是异常。每一个 `promise` 都与一个 `ThreadContext` 绑定，意味着该异步任务运行在这种上下文中。

# 使用指南
## 创建 `Promise` 实例
```c++
using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

auto promise = Promise<void>::New([]{
  // 在std::thread环境下运行您的task
}, STLThreadContext::New());
```

## 创建延迟满足的 `promise`
```c++
// libdispatch
using CurrentThreadContext = ThreadContextImpl::GCD::CurrentThreadContext;

auto promise = Promise<int>::New([](PromiseDefer<int>&& defer){
  // 在当前线程环境运行异步的代码，只有在异步代码完成后，promise才算满足
  doSomeAsync([deferred = std::move(defer)] (int code) mutable {
    // 完成时
    if (code < 0) {
        // 处理错误
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

## 创建嵌套 `promise`
```c++
// Win32
using ThreadPoolThreadContext = ThreadContextImpl::Windows::ThreadPoolContext; 

// 当前promise满足当且嵌套的promise满足
auto promise = Promise<int>::New([]() -> Promise<int> /* 返回一个 promise */ {
  // 运行在win32的线程池中
  return Promise<int>::New([]{
    return 1024;
  }, ThreadPoolThreadContext::New());
  
}, ThreadPoolThreadContext::New());

```

## 将异步流串联起来
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

## 串联延迟满足的 ｀promise｀
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

## 串联嵌套的 `promise`
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

## 直接满足和拒绝
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

### 与 Objective-c++ 混合使用
- 在Xcode中打开 `ARC` (**如果没有于Xcode一起使用，则添加编译器参数 `-fobjc-arc` **)
- 该更实现文件后缀为 `.mm`
- 与objc的block一起使用时，应添加 `__block` 关键字至c++对象前

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

# License
  MIT License



