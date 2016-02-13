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

namespace SpecFixedValue {
  void passUserException(std::exception_ptr e) {
    if (e) {
      try {
        std::rethrow_exception(e);
      } catch(const UserException&) {
        // pass
      } catch(...) {
        throw AssertionFailed();
      }
    } else {
      throw AssertionFailed();
    }
  }

#define INIT(context, tag) \
    spec \
     /* ==> */ \
    .it(#tag"should acquire the fulfilled value", []{ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::Resolved(truth).then([=](bool fulfilled){ \
        if (truth != fulfilled) \
          throw AssertionFailed(); \
      }, [](std::exception_ptr) { \
        throw AssertionFailed(); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream", []{ \
      Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())).then([](bool){ \
        throw AssertionFailed(); \
      }, passUserException, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from returned task", []{ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::New([=]{ return truth; }, context::New()).then([=](bool fulfilled){ \
        if (truth != fulfilled) \
            throw AssertionFailed(); \
      }, [](std::exception_ptr) { \
          throw AssertionFailed(); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream from returned task", []{ \
      Promise2::Promise<bool>::New([]() -> bool { throw UserException(); }, context::New()).then([](bool){ \
        throw AssertionFailed(); \
      }, passUserException, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from nesting promise", []{ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::New([=]{ return Promise2::Promise<bool>::Resolved(truth); }, context::New()).then([=](bool fulfilled){ \
        if (truth != fulfilled) \
            throw AssertionFailed(); \
      }, [](std::exception_ptr) { \
          throw AssertionFailed(); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream from nesting promise", []{ \
      Promise2::Promise<bool>::New([]{ return Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())); }, context::New()).then([=](bool fulfilled){ \
       throw AssertionFailed(); \
      }, passUserException, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from deferred promise", []{ \
      constexpr bool truth = true; \
      Promise2::Promise<bool>::New([=](Promise2::PromiseDefer<bool>&& deferred){ deferred.setResult(truth); }, context::New()).then([=](bool fulfilled){ \
        if (truth != fulfilled) \
            throw AssertionFailed(); \
      }, [](std::exception_ptr) { \
          throw AssertionFailed(); \
      }, context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should transfer the exception downstream from deferred promise", []{ \
      Promise2::Promise<bool>::New([](Promise2::PromiseDefer<bool>&& deferred){ deferred.setException(std::make_exception_ptr(UserException())); }, context::New()).then([=](bool fulfilled){ \
       throw AssertionFailed(); \
      }, passUserException, context::New()); \
    });

  template<typename T> \
  void init(T& spec) { \
    INIT(CurrentContext, CurrentContext:)
    INIT(ThreadContextImpl::STL::DetachedThreadContext, STLDetached:)
  // end of the init spec
  }
} // SpecFixedValue

TEST_ENTRY(SPEC_TFN(SpecFixedValue::init))

#endif // PROMISE2_API_CPP
