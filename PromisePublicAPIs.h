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
 
namespace Promise2 {
	// declarations
	namespace Details {
		template<typename T> class PromiseNode;
		template<typename T> class Forward;
    template<typename T> using DeferPromiseCore = std::unique_ptr<Forward<T>>;
	} // Details
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
    PromiseDefer(Details::DeferPromiseCore<T>&& core);

    PromiseDefer(PromiseDefer<T>&&) = default;
    ~PromiseDefer() = default;

  public:
    template<typename X> void setResult(X&& r);

    void setException(std::exception_ptr e);

  private:
    PromiseDefer(const PromiseDefer<T>&) = delete;
    PromiseDefer& operator = (const PromiseDefer<T>&) = delete;
  };

  //
  // @class Promise
  //
  template<typename T>
  class Promise {
  private:
    std::shared_ptr<Details::PromiseNode<T>> _node; 

  public:
    // empty constructor
    Promise() = default;
    
    // constructor with task and running context
    Promise(std::function<T(void)>&& task, ThreadContext* &&context);

    // constructor with deferred task
    Promise(std::function<void(PromiseDefer<T>&&)>&& task, ThreadContext* &&context);

  	Promise(Promise<T>&& promise);

  	Promise<T>& operator = (Promise<T>&& promise);
  	
	public:
		template<typename NextT>
    Promise<NextT> then(std::function<NextT(T)>&& onFulfill, 
                        std::function<void(std::exception_ptr)>&& onReject, 
                        ThreadContext* &&context);

    template<typename NextT>
    Promise<NextT> then(std::function<void(PromiseDefer<NextT>&&, T)>&& onFulfill,
                        std::function<void(std::exception_ptr)>&& onReject, 
                        ThreadContext* &&context);

  public:
    bool isValid() const {
      return !!_node;
    }

  private:
    Promise(const Promise<T>& ) = delete;
    Promise& operator = (const Promise<T>& ) = delete;
  };
}
 
 #endif // PROMISE_PUBLIC_APIS_H
 