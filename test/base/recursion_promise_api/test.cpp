/*
 * Promise2
 *
 * Copyright (c) 2015-2016 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */

#include <atomic>
#include <vector>

#include "entry.h"
#include "Promise.h"
#include "context/ThreadContext_STL.h"

class CurrentContext : public Promise2::ThreadContext {
public:
  static ThreadContext *New() {
    return new CurrentContext;
  }

public:
  virtual void scheduleToRun(std::function<void()>&& task) override {
    task(); 
  }
};

using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

namespace RecursionAPIsBase {

  // IntputIterator
  class UserIterator {
  public:
    static constexpr const std::int32_t max = 3;

  private:
    std::int32_t _index;

  public:
    UserIterator(bool end)
      : _index { end? 3 : 0 }
    {}

  public:
    bool operator == (const UserIterator& i) { return i._index == _index; }
    // must
    bool operator != (const UserIterator& i) { return i._index != _index; }

    // must
    UserIterator& operator++ () {
      ++_index;
      return *this;
    }

    // must
    std::int32_t operator *() const {
      return _index;
    }
  };

  class UserExceptionIterator {
  public:
    static constexpr const std::int32_t max = 3;

  private:
    std::int32_t _index;

  public:
    UserExceptionIterator(bool end)
      : _index { end? 3 : 0 }
    {}

  public:
    bool operator == (const UserExceptionIterator& i) { return i._index == _index; }
    // must
    bool operator != (const UserExceptionIterator& i) { return i._index != _index; }

    // must
    UserExceptionIterator& operator++ () {
      ++_index;
      return *this;
    }

    // must
    std::int32_t operator *() const {
      if (_index > 0) {
        throw UserException();
      }

      return _index;
    }
  };

  constexpr const std::int32_t UserIterator::max;

  template<typename T>
  void init(T& spec) {
    using context = CurrentContext;

    spec
    /* ==> */ 
    .it("should run three times", [](const LTest::SharedCaseEndNotifier& notifier){
      std::vector<std::int32_t> values = {1, 2, 3};
      auto len = values.size();

      auto counter = std::make_shared<std::int32_t>(0);

      Promise2::RecursionPromise<std::int32_t>::Iterate(values.begin(), values.end(), new context()).
      then([=](std::int32_t ) { ++*counter; }, 
           [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::RecursionPromise<void>(); },
           new context()).
      final([=]() { if (len == *counter) { notifier->done(); }
                    else notifier->fail(std::make_exception_ptr(AssertionFailed())); }, 
            [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::Promise<void>(); },
            new context());
    })
    /* ==> */ 
    .it("should run three times with customized iterator", [](const LTest::SharedCaseEndNotifier& notifier){
      auto counter = std::make_shared<std::int32_t>(0);
      
      Promise2::RecursionPromise<std::int32_t>::Iterate(UserIterator(false), UserIterator(true), new context()).
      then([=](std::int32_t ) { ++*counter; }, 
           [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::RecursionPromise<void>(); },
           new context()).
      final([=]() { if (UserIterator::max == *counter) { notifier->done(); }
                    else notifier->fail(std::make_exception_ptr(AssertionFailed())); }, 
            [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::Promise<void>(); },
            new context());
    })
    /* ==> */ 
    .it("should run only one time when deref throws exception", [](const LTest::SharedCaseEndNotifier& notifier){
      auto counter = std::make_shared<std::int32_t>(0);
      
      Promise2::RecursionPromise<std::int32_t>::Iterate(UserExceptionIterator(false), UserExceptionIterator(true), new context()).
      then([=](std::int32_t ) { ++*counter; }, 
           [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::RecursionPromise<void>(); },
           new context()).
      final([=]() { notifier->fail(std::make_exception_ptr(AssertionFailed())); }, 
            [=](std::exception_ptr e) { if (1 == *counter ) { notifier->done(); }
                                       else { notifier->fail(std::make_exception_ptr(AssertionFailed())); }
                                       return Promise2::Promise<void>::Rejected(e); },
            new context());
    })
    /* ==> */ 
    .it("should be fulfilled when finished", [](const LTest::SharedCaseEndNotifier& notifier){      
      auto p = Promise2::RecursionPromise<std::int32_t>::Iterate(UserIterator(false), UserIterator(true), new context());

      p.
      then([=](std::int32_t ) {},
           [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::RecursionPromise<void>(); },
           new context()).
      final([=]() { if (p.isFulfilled()) notifier->done(); }, 
            [=](std::exception_ptr e) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::Promise<void>(); },
            new context());
    })
    /* ==> */ 
    .it("should be rejected when finished", [](const LTest::SharedCaseEndNotifier& notifier){      
      auto p = Promise2::RecursionPromise<std::int32_t>::Iterate(UserExceptionIterator(false), UserExceptionIterator(true), new context());

      p.
      then([=](std::int32_t ) {
          if (p.isFulfilled()) { notifier->fail(std::make_exception_ptr(AssertionFailed())); } },
           [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::RecursionPromise<void>(); },
           new context()).
      final([=]() { notifier->fail(std::make_exception_ptr(AssertionFailed())); }, 
            [=](std::exception_ptr e) { if (p.isRejected()) notifier->done(); return Promise2::Promise<void>::Rejected(e); },
            new context());
    })
    /* ==> */
    .it("should run three times with customized iterator under STL thread context", [](const LTest::SharedCaseEndNotifier& notifier){
      auto counter = std::make_shared<std::int32_t>(0);
      
      Promise2::RecursionPromise<std::int32_t>::Iterate(UserIterator(false), UserIterator(true), STLThreadContext::New()).
      then([=](std::int32_t ) { ++*counter; }, 
           [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::RecursionPromise<void>(); },
          STLThreadContext::New()).
      final([=]() { if (UserIterator::max == *counter) { notifier->done(); }
                    else notifier->fail(std::make_exception_ptr(AssertionFailed())); }, 
            [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::Promise<void>(); },
            STLThreadContext::New());
    })
    /* ==> */ 
    .it("should run only one time when deref throws exception under STL thread context", [](const LTest::SharedCaseEndNotifier& notifier){
      auto counter = std::make_shared<std::int32_t>(0);
      
      Promise2::RecursionPromise<std::int32_t>::Iterate(UserExceptionIterator(false), UserExceptionIterator(true), STLThreadContext::New()).
      then([=](std::int32_t ) { ++*counter; }, 
           [=](std::exception_ptr) { notifier->fail(std::make_exception_ptr(AssertionFailed())); return Promise2::RecursionPromise<void>(); },
           STLThreadContext::New()).
      final([=]() { notifier->fail(std::make_exception_ptr(AssertionFailed())); }, 
            [=](std::exception_ptr e) { if (1 == *counter ) { notifier->done(); }
                                       else { notifier->fail(std::make_exception_ptr(AssertionFailed())); }
                                       return Promise2::Promise<void>::Rejected(e); },
            STLThreadContext::New());
    });
  }

} // RecursionAPIsBase

TEST_ENTRY(CONTAINER_TYPE,
  SPEC_TFN(RecursionAPIsBase::init));
