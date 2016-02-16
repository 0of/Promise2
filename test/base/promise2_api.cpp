/*
 * Promise2
 *
 * Copyright (c) 2015-2016 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */
#ifndef PROMISE2_API_CPP
#define PROMISE2_API_CPP

#define NESTING_PROMISE 1
#define DEFERRED_PROMISE 1
/*
 * testing APIs
 */
#include "Promise.h"
#include "entry.h"
#include "ThreadContext_STL.h"

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

class UserException : public std::exception {};
class AssertionFailed : public std::exception {};

using STLThreadContext = ThreadContextImpl::STL::DetachedThreadContext;

namespace SpecFixedValue {
  void passUserException(const LTest::SharedCaseEndNotifier& notifier, std::exception_ptr e) {
    if (e) {
      try {
        std::rethrow_exception(e);
      } catch(const UserException&) {
        // pass
        notifier->done();
      } catch(...) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }
    } else {
      notifier->fail(std::make_exception_ptr(AssertionFailed()));
    }
  }

#define INIT(context, tag) \
    spec \
     /* ==> */ \
    .it(#tag"should acquire the fulfilled value", [](const LTest::SharedCaseEndNotifier& notifier){ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::Resolved(truth).then([=](bool fulfilled){ \
        if (truth != fulfilled) { \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
          return; \
        } \
        notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream", [](const LTest::SharedCaseEndNotifier& notifier){ \
      Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())).then([=](bool){ \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, std::bind(passUserException, notifier, std::placeholders::_1), context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from returned task", [](const LTest::SharedCaseEndNotifier& notifier){ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::New([=]{ return truth; }, context::New()).then([=](bool fulfilled){ \
        if (truth != fulfilled) { \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
          return; \
        } \
        notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream from returned task", [](const LTest::SharedCaseEndNotifier& notifier){ \
      Promise2::Promise<bool>::New([]() -> bool { throw UserException(); }, context::New()).then([=](bool){ \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, std::bind(passUserException, notifier, std::placeholders::_1), context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier){ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::New([=]{ return Promise2::Promise<bool>::Resolved(truth); }, context::New()).then([=](bool fulfilled){ \
        if (truth != fulfilled) { \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
          return; \
        } \
        notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier){ \
      Promise2::Promise<bool>::New([]{ \
        return Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())); \
      }, context::New()).then([=](bool fulfilled){ \
       notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, std::bind(passUserException, notifier, std::placeholders::_1), context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier){ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::New([=](Promise2::PromiseDefer<bool>&& deferred){ \
        deferred.setResult(truth); \
      }, context::New()).then([=](bool fulfilled){ \
        if (truth != fulfilled) { \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
          return; \
        } \
        notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier){ \
      Promise2::Promise<bool>::New([](Promise2::PromiseDefer<bool>&& deferred){ \
        deferred.setException(std::make_exception_ptr(UserException())); \
      }, context::New()).then([=](bool fulfilled){ \
       notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, std::bind(passUserException, notifier, std::placeholders::_1), context::New()); \
    });
    
  template<typename T>
  void init(T& spec) {
    INIT(CurrentContext, CurrentContext:)
    INIT(STLThreadContext, STLThreadContext:)
  // end of the init spec
  }
} // SpecFixedValue

TEST_ENTRY(SPEC_TFN(SpecFixedValue::init))

#endif // PROMISE2_API_CPP
