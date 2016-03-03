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
#ifdef __APPLE__
# define USE_DISPATCH 1
#endif // __APPLE__

#include <cstdint>
#include <cmath>
#include <climits>

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

#ifdef __APPLE__
#include <cstdlib>
#include <dispatch/dispatch.h>

class Voidness : public LTest::TestRunable {
public:
  virtual void run(LTest::TestRunnableContainer& container) noexcept override {}
};

class GCDContainer : public DefaultContainer {
private:
  std::atomic_int _runningTestCount;

private:
  using Runnable = std::pair<GCDContainer *, LTest::SharedTestRunnable>;

  static void InvokeRunnable(void *context) {
    std::unique_ptr<Runnable> runnable{ static_cast<Runnable *>(context) };
    runnable->second->run(*runnable->first);
  }

  static void quit(void *) {
    std::exit(0);
  }

public:
  GCDContainer()
    : _runningTestCount{ 0 } {
    // DefaultContainer's got something to run with so that it can invoke the `startTheLoop` within `start`
    DefaultContainer::scheduleToRun(std::make_shared<Voidness>());
  }

public:
  virtual void scheduleToRun(const LTest::SharedTestRunnable& runnable) override {
    dispatch_async_f(dispatch_get_main_queue(), new Runnable{ this, runnable }, InvokeRunnable);
    ++_runningTestCount;
  }

  virtual void endRun() override {
    DefaultContainer::endRun();

    if (--_runningTestCount == 0) {
        dispatch_async_f(dispatch_get_main_queue(), nullptr, quit);
    }
  }

protected:
  virtual void startTheLoop() override {
    dispatch_main();
  }
};
# define CONTAINER_TYPE GCDContainer

#include "ThreadContext_GCD.h"
using MainThreadContext = ThreadContextImpl::GCD::MainThreadContext;

#else
# define CONTAINER_TYPE DefaultContainer
#endif // __APPLE__

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
#ifdef __APPLE__
    INIT(MainThreadContext, GCDThreadContext:)
#endif // __APPLE__
  // end of the init spec
  }
} // SpecFixedValue

namespace PromiseAPIsBase {
  template<typename T>
  void init(T& spec) {
    spec
    /* ==> */
    .it("should be fulfilled", [] {
      auto p = Promise2::Promise<int>::Resolved(1);
      if (!p.isFulfilled())
        throw AssertionFailed();
    })
    /* ==> */
    .it("should be rejected", [] {
      auto p = Promise2::Promise<int>::Rejected(std::make_exception_ptr(UserException()));
      if (!p.isRejected())
        throw AssertionFailed();
    })
    /* ==> */
    .it("should be fulfilled when the task returned", [](const LTest::SharedCaseEndNotifier& notifier) {
      constexpr bool truth = true;
      auto p = Promise2::Promise<bool>::New([=] { return truth; }, CurrentContext::New());
      p.then([=](bool) {
        if (!p.isFulfilled())
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
        else
          notifier->done();
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, CurrentContext::New());
    })
    /* ==> */
    .it("should be rejected when the task returned", [](const LTest::SharedCaseEndNotifier& notifier) {
      auto p = Promise2::Promise<bool>::New([]() -> bool { throw UserException(); }, CurrentContext::New());
      p.then([=](bool) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr) {
        if (!p.isRejected())
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
        else
          notifier->done();
      }, CurrentContext::New());
    })
    /* ==> */
    .it("should be fulfilled when the deferred task returned", [](const LTest::SharedCaseEndNotifier& notifier) {
      constexpr bool truth = true;
      auto p = Promise2::Promise<bool>::New([=](Promise2::PromiseDefer<bool>&& deferred) {
        deferred.setResult(truth);
      }, CurrentContext::New());

      p.then([=](bool) {
        if (!p.isFulfilled())
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
        else
          notifier->done();
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, CurrentContext::New());
    })
    /* ==> */
    .it("should be rejected when the task returned", [](const LTest::SharedCaseEndNotifier& notifier) {
      auto p = Promise2::Promise<bool>::New([](Promise2::PromiseDefer<bool>&& deferred) {
        deferred.setException(std::make_exception_ptr(UserException()));
      }, CurrentContext::New());

      p.then([=](bool) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr) {
        if (!p.isRejected())
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
        else
          notifier->done();
      }, CurrentContext::New());
    })
    /* ==> */
    .it("should be fulfilled from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) {
      constexpr bool truth = true;
      auto p = Promise2::Promise<bool>::New([=]() {
        return Promise2::Promise<bool>::Resolved(truth);
      }, CurrentContext::New());

      p.then([=](bool) {
        if (!p.isFulfilled())
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
        else
          notifier->done();
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, CurrentContext::New());
    })
    /* ==> */
    .it("should be rejected from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) {
      auto p = Promise2::Promise<bool>::New([] {
        return Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException()));
      }, CurrentContext::New());

      p.then([=](bool) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr) {
        if (!p.isRejected())
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
        else
          notifier->done();
      }, CurrentContext::New());
    })
    /* ==> */
    .it("should throw logic exception when calling `then` upon invalid promise", [] {
      Promise2::Promise<bool> p;

      try {
        p.then([=](bool) {}, [=](std::exception_ptr) {}, CurrentContext::New());
      } catch (const std::logic_error&) {
        // pass
        return;
      }

      throw AssertionFailed();
    })
    .it("should throw logic exception when calling `isRejected` upon invalid promise", [] {
      Promise2::Promise<bool> p;

      try {
        p.isRejected();
      } catch (const std::logic_error&) {
        // pass
        return;
      }

      throw AssertionFailed();
    })
    .it("should throw logic exception when calling `isFulfilled` upon invalid promise", [] {
      Promise2::Promise<bool> p;

      try {
        p.isFulfilled();
      }
      catch (const std::logic_error&) {
        // pass
        return;
      }

      throw AssertionFailed();
    });
    // end of the init spec
  }
}

namespace DataValidate {
  // trivial type
  struct TrivialDataType {
  protected:
    std::int32_t m1;
  public:
    std::float_t m2;
    std::double_t m3;
    std::int64_t m4;
    std::int8_t *m5;

    void setM1(std::int32_t v) { m1 = v; }
    std::int32_t getM1() const { return m1; }
  };

  // pod type
  struct PodDataType {
    std::int32_t m1;
    std::float_t m2;
    std::double_t m3;
    std::int64_t m4;
    std::int8_t *m5;
  };

  // standard layout
  struct StandardLayoutDataType {
    std::int32_t m1;
    std::float_t m2;
    std::double_t m3;
    std::int64_t m4;
    std::int8_t *m5;

    StandardLayoutDataType()
      : m1(5) {}
  };

  // normal class
  class NormalClass {
  private:

  };

  // virtual methods class
  class VirtualMethodClass {

  };

  template<typename T>
  void initCommonData(T& data) {
    data.m2 = 2.f;
    data.m3 = 32.;
    data.m4 = 129;
    data.m5 = nullptr;
  }

  template<typename T>
  bool validateCommonData(const T& data) {
    return (std::fabs(data.m2 - 2.f) < std::numeric_limits<decltype(data.m2)>::epsilon()) &&
           (std::fabs(data.m3 - 32.) < std::numeric_limits<decltype(data.m3)>::epsilon()) &&
            data.m4 == 129 &&
            data.m5 == nullptr;
  }

  void initTrivialDataType(TrivialDataType& trivialData) {
    trivialData.setM1(5);
    initCommonData(trivialData);
  }

  void initPodDataType(PodDataType& podData) {
    podData.m1 = 5;
    initCommonData(podData);
  }

  bool validateTrivialDataType(const TrivialDataType& trivialData) {
    return trivialData.getM1() == 5 && validateCommonData(trivialData);
  }

  template<typename StandardLayoutType>
  bool validateStandardLayoutDataType(const StandardLayoutType& data) {
    return data.m1 == 5 && validateCommonData(data);
  }

#define DATATEST_INIT(context, tag) \
    spec \
     /* ==> */ \
    .it(#tag"should fulfill trivial type instance correctly", [](const LTest::SharedCaseEndNotifier& notifier){ \
      Promise2::Promise<TrivialDataType>::New([] { \
        TrivialDataType data; \
        initTrivialDataType(data); \
        return data; \
      }, context::New()).then([=](TrivialDataType data) { \
        if (!validateTrivialDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill trivial type instance correctly from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      Promise2::Promise<TrivialDataType>::New([](Promise2::PromiseDefer<TrivialDataType>&& deferred) { \
        TrivialDataType data; \
        initTrivialDataType(data); \
        deferred.setResult(data); \
      }, context::New()).then([=](TrivialDataType data) { \
        if (!validateTrivialDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill trivial type instance correctly from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      Promise2::Promise<TrivialDataType>::New([]() { \
        TrivialDataType data; \
        initTrivialDataType(data); \
        return Promise2::Promise<TrivialDataType>::Resolved(data); \
      }, context::New()).then([=](TrivialDataType data) { \
        if (!validateTrivialDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill pod type instance correctly", [](const LTest::SharedCaseEndNotifier& notifier){ \
      Promise2::Promise<PodDataType>::New([] { \
        PodDataType data; \
        initPodDataType(data); \
        return data; \
      }, context::New()).then([=](PodDataType data) { \
        if (!validateStandardLayoutDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill pod type instance correctly from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      Promise2::Promise<PodDataType>::New([](Promise2::PromiseDefer<PodDataType>&& deferred) { \
        PodDataType data; \
        initPodDataType(data); \
        deferred.setResult(data); \
      }, context::New()).then([=](PodDataType data) { \
        if (!validateStandardLayoutDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill pod type instance correctly from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      Promise2::Promise<PodDataType>::New([]() { \
        PodDataType data; \
        initPodDataType(data); \
        return Promise2::Promise<PodDataType>::Resolved(data); \
      }, context::New()).then([=](PodDataType data) { \
        if (!validateStandardLayoutDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill standard layout type instance correctly", [](const LTest::SharedCaseEndNotifier& notifier){ \
      Promise2::Promise<StandardLayoutDataType>::New([] { \
        StandardLayoutDataType data; \
        initCommonData(data); \
        return data; \
      }, context::New()).then([=](StandardLayoutDataType data) { \
        if (!validateStandardLayoutDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill standard layout type instance correctly from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      Promise2::Promise<StandardLayoutDataType>::New([](Promise2::PromiseDefer<StandardLayoutDataType>&& deferred) { \
        StandardLayoutDataType data; \
        initCommonData(data); \
        deferred.setResult(data); \
      }, context::New()).then([=](StandardLayoutDataType data) { \
        if (!validateStandardLayoutDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill standard layout type instance correctly from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      Promise2::Promise<StandardLayoutDataType>::New([]() { \
        StandardLayoutDataType data; \
        initCommonData(data); \
        return Promise2::Promise<StandardLayoutDataType>::Resolved(data); \
      }, context::New()).then([=](StandardLayoutDataType data) { \
        if (!validateStandardLayoutDataType(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    });

  template<typename T>
  void init(T& spec) {
    DATATEST_INIT(CurrentContext, CurrentContext:)
    DATATEST_INIT(STLThreadContext, STLThreadContext:)
#ifdef __APPLE__
    DATATEST_INIT(MainThreadContext, GCDThreadContext:)
#endif // __APPLE__
    // raw pointer
    // single numbers
    // string
    // shared_ptr
  }
}

TEST_ENTRY(CONTAINER_TYPE,
  SPEC_TFN(SpecFixedValue::init),
  SPEC_TFN(PromiseAPIsBase::init),
  SPEC_TFN(DataValidate::init))

#endif // PROMISE2_API_CPP
