/*
* Promise2
*
* Copyright (c) 2015 "0of" Magnus
* Licensed under the MIT license.
* https://github.com/0of/Promise2/blob/master/LICENSE
*/

#ifndef PROMISE_PUBLIC_APIS_H
#define PROMISE_PUBLIC_APIS_H

#include <functional>
#include <memory>
#include <exception>
#include <stdexcept>
 
#include "PromiseConfig.h"
#include "declfn.h"

namespace Promise2 {
  // declarations
  namespace Details {
    template<typename T> class PromiseNode;
    template<typename T> class Forward;
    template<typename T> using DeferPromiseCore = std::shared_ptr<Forward<T>>;
  } // Details

  template<typename T> class Promise;
  template<typename T> using SharedPromiseNode = std::shared_ptr<Details::PromiseNode<T>>;
  template<typename T> using OnRejectFunction = std::function<Promise<T>(std::exception_ptr)>;
  // !

  //
  // @class ThreadContext
  //
  class ThreadContext {
  public:
    virtual ~ThreadContext() = default;

  public:
    virtual void scheduleToRun(std::function<void()>&& task) = 0;
  };

  //
  // @class PromiseDefer
  //
  template<typename T>
  class PromiseDefer {
  private:
    Details::DeferPromiseCore<T> _core;

  public:
    PromiseDefer(Details::DeferPromiseCore<T>& core);

    PromiseDefer(PromiseDefer<T>&&) = default;
    ~PromiseDefer() = default;

  public:
    template<typename X> void setResult(X&& r);

    void setException(std::exception_ptr e);

  private:
    PromiseDefer(const PromiseDefer<T>&) = delete;
    PromiseDefer& operator = (const PromiseDefer<T>&) = delete;
  };

  template<>
  class PromiseDefer<void> {
  private:
    Details::DeferPromiseCore<void> _core;

  public:
    PromiseDefer(Details::DeferPromiseCore<void>& core);

    PromiseDefer(PromiseDefer<void>&&) = default;
    ~PromiseDefer() = default;

  public:
    void setResult();

    void setException(std::exception_ptr e);

  private:
    PromiseDefer(const PromiseDefer<void>&) = delete;
    PromiseDefer<void>& operator = (const PromiseDefer<void>&) = delete;
  };

  template<typename T>
  class PromiseSpawner {
  public:
    template<typename Task>
    static Promise<T> New(Task&& task, ThreadContext* &&context) {
      auto taskFn = declfn(task){ std::move(task) };

      static_assert(!std::is_same<decltype(taskFn), std::false_type>::value, "you need to provide a callable");
      static_assert(!std::is_same<decltype(Spawn(std::move(taskFn), std::move(context))), std::false_type>::value, "match nothing...");

      return std::move(Spawn(std::move(taskFn), std::move(context)));
    }

  private:
    // constructor with task and running context
    static Promise<T> Spawn(std::function<T(void)>&& task, ThreadContext* &&context);

    // constructor with deferred task
    static Promise<T> Spawn(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context);

    // constructor with nesting promise task
    static Promise<T> Spawn(std::function<Promise<T>()>&& task, ThreadContext* &&context);

    // matching nothing
    template<typename NonMatching> static auto Spawn(NonMatching&&, ThreadContext* &&) -> std::false_type;
  };

  template<typename T>
  class PromiseThenable {
  public:
    template<typename NextT>
    static Promise<NextT> Then(SharedPromiseNode<T>& node,
                               std::function<NextT(T)>&& onFulfill, 
                               OnRejectFunction<NextT>&& onReject, 
                               ThreadContext* &&context);

    template<typename NextT>
    static Promise<NextT> Then(SharedPromiseNode<T>& node,
                               std::function<void(PromiseDefer<NextT>&&, T)>&& onFulfill, 
                               OnRejectFunction<NextT>&& onReject, 
                               ThreadContext* &&context);

    template<typename NextT>
    static Promise<NextT> Then(SharedPromiseNode<T>& node, 
                               std::function<Promise<NextT>(T)>&& onFulfill,
                               OnRejectFunction<NextT>&& onReject, 
                               ThreadContext* &&context);

    // matching nothing
    template<typename NonMatching> static auto Then(...) -> std::false_type;
  };

  template<>
  class PromiseThenable<void> {
  public:
    template<typename NextT>
    static Promise<NextT> Then(SharedPromiseNode<void>& node,
                               std::function<NextT(void)>&& onFulfill, 
                               OnRejectFunction<NextT>&& onReject,
                               ThreadContext* &&context);

    template<typename NextT>
    static Promise<NextT> Then(SharedPromiseNode<void>& node,
                               std::function<void(PromiseDefer<NextT>&&)>&& onFulfill, 
                               OnRejectFunction<NextT>&& onReject,
                               ThreadContext* &&context);

    template<typename NextT>
    static Promise<NextT> Then(SharedPromiseNode<void>& node, 
                               std::function<Promise<NextT>(void)>&& onFulfill,
                               OnRejectFunction<NextT>&& onReject,
                               ThreadContext* &&context);

    // matching nothing
    template<typename NonMatching> static auto Then(...) -> std::false_type;
  };

  //
  // @class Promise
  //
  template<typename T>
  class Promise : public PromiseSpawner<T> {
    template<typename Type> friend class PromiseThenable;

  private:    
    using Thenable = PromiseThenable<T>;
    using SelfType = Promise<T>;

    friend class PromiseSpawner<T>;

  public:
    using PromiseType = T;

  public:
    template<typename ArgType>
    static Promise<T> Resolved(ArgType&& arg);
    static Promise<T> Rejected(std::exception_ptr e);

  private:
    SharedPromiseNode<T> _node;

  public:
    // empty constructor
    Promise() = default;

    // copy constructor
    Promise(const SelfType& ) = default;
    // move constructor
    Promise(SelfType&& ) = default;

    SelfType& operator = (const SelfType& ) = default;
    SelfType& operator = (SelfType&& ) = default;

  public:
    template<typename OnFulfill, typename OnReject>
    auto then(OnFulfill&& onFulfill,
              OnReject&& onReject, 
              ThreadContext* &&context) {
      static_assert(!std::is_same<declfn(onFulfill), std::false_type>::value &&
                    !std::is_same<declfn(onReject), std::false_type>::value , "you need to provide a callable");

      auto onFulfillFn = declfn(onFulfill){ std::move(onFulfill) };
      auto onRejectFn = declfn(onReject){ std::move(onReject) };

      static_assert(!std::is_same<decltype(Thenable::Then(_node, std::move(onFulfillFn), std::move(onRejectFn), std::move(context))), std::false_type>::value, "match nothing...");

      if (!isValid()) throw std::logic_error("invalid promise");
      return Thenable::Then(_node, std::move(onFulfillFn), std::move(onRejectFn), std::move(context));
    }
    
  public:
    bool isValid() const {
      return !!_node;
    }

  public:
    bool isFulfilled() const; 
    bool isRejected() const;

  public:
    inline SharedPromiseNode<T> internal() const { return _node; }
  };

  template<>
  class Promise<void> : public PromiseSpawner<void> {
    template<typename Type> friend class PromiseThenable;

  private:
    friend class PromiseSpawner<void>;

    using Thenable = PromiseThenable<void>;
    using SelfType = Promise<void>;

  public:
    using PromiseType = void;

  public:
    static SelfType Resolved();
    static SelfType Rejected(std::exception_ptr e);

  private:
    SharedPromiseNode<void> _node; 

  public:
    // empty constructor
    Promise() = default;

    // copy constructor
    Promise(const SelfType& ) = default;
    // move constructor
    Promise(SelfType&& promise) = default;

    SelfType& operator = (const SelfType& ) = default;
    SelfType& operator = (SelfType&& ) = default;

  public:
    template<typename OnFulfill, typename OnReject>
    auto then(OnFulfill&& onFulfill,
              OnReject&& onReject, 
              ThreadContext* &&context) {
      auto onFulfillFn = declfn(onFulfill){ std::move(onFulfill) };
      auto onRejectFn = declfn(onReject){ std::move(onReject) };

      static_assert(!std::is_same<decltype(onFulfillFn), std::false_type>::value &&
                    !std::is_same<decltype(onRejectFn), std::false_type>::value, "you need to provide a callable");

      static_assert(!std::is_same<decltype(Thenable::Then(_node, std::move(onFulfillFn), std::move(onRejectFn), std::move(context))), std::false_type>::value, "match nothing...");

      if (!isValid()) throw std::logic_error("invalid promise");
      return Thenable::Then(_node, std::move(onFulfillFn), std::move(onRejectFn), std::move(context));
    }

  public:
    bool isValid() const {
      return !!_node;
    }

    bool isFulfilled() const;
    bool isRejected() const;

  public:
    inline SharedPromiseNode<void> internal() const { return _node; }
  };
}
 
#endif // PROMISE_PUBLIC_APIS_H
 