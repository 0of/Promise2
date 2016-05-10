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
#include <algorithm>
#include <string>
#include <vector>

/*
 * testing APIs
 */
#include "Promise.h"
#include "entry.h"
#include "context/ThreadContext_STL.h"

#ifdef __APPLE__
 #include "context/ThreadContext_GCD.h"
using MainThreadContext = ThreadContextImpl::GCD::MainThreadContext;
#endif // __APPLE__

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

template<typename T>
class WrappedOnRejectPromise;

template<typename OnRejectReturnType>
struct DoWrapOnReject {
  template<typename OnReject>
  static auto wrap(OnReject&& onReject) {
    return std::move(onReject);
  }
};

template<>
struct DoWrapOnReject<void> {
  template<typename OnReject>
  static auto wrap(OnReject&& onReject) {
    auto wrappedOnRejected = [reject = std::move(onReject)] (std::exception_ptr e) {
      reject(e);
      return Promise2::Promise<void>::Resolved();
    };
    return std::move(wrappedOnRejected);
  }
};

template<typename T>
struct WrappedPromiseResolved {
  template<typename ArgType>
  static WrappedOnRejectPromise<T> Resolved(ArgType&& arg);

  static WrappedOnRejectPromise<T> Resolved();
};

template<>
struct WrappedPromiseResolved<void> {
  template<typename ArgType>
  static WrappedOnRejectPromise<void> Resolved(ArgType&& arg);

  static WrappedOnRejectPromise<void> Resolved();
};

//
// bind `OnReject` function to new signature `OnReject -> std::exception_ptr -> Promise<T>`
//
template<typename T>
class WrappedOnRejectPromise : public Promise2::Promise<T> {
public:
  template<typename Task>
  static WrappedOnRejectPromise<T> New(Task&& task, Promise2::ThreadContext* &&context) {
    auto promise = Promise2::Promise<T>::New(std::move(task), std::move(context));
    return WrappedOnRejectPromise<T>{ promise };
  }

  template<typename ArgType>
  static WrappedOnRejectPromise<T> Resolved(ArgType&& arg) {
    return WrappedPromiseResolved<T>::Resolved(std::forward<ArgType>(arg));
  }

  static WrappedOnRejectPromise<T> Resolved() {
    return WrappedPromiseResolved<T>::Resolved();
  }

  static WrappedOnRejectPromise<T> Rejected(std::exception_ptr e) {
    auto promise = Promise2::Promise<T>::Rejected(e);
    return WrappedOnRejectPromise<T>{ promise };
  }

public:
  WrappedOnRejectPromise() = default;
  WrappedOnRejectPromise(const Promise2::Promise<T>& p)
    : Promise2::Promise<T>(p)
  {}
    
public:
  template<typename OnFulfill, typename OnReject>
  auto then(OnFulfill&& onFulfill,
            OnReject&& onReject,
            Promise2::ThreadContext* &&context) {
    auto wrappedOnRejected = DoWrapOnReject<typename declfn(onReject)::result_type>::wrap(std::move(onReject));
    auto promise = Promise2::Promise<T>::then(std::move(onFulfill), std::move(wrappedOnRejected), std::move(context));

    return WrappedOnRejectPromise<typename decltype(promise)::PromiseType>{ promise };
  }
};

template<typename T>
template<typename ArgType>
WrappedOnRejectPromise<T> WrappedPromiseResolved<T>::Resolved(ArgType&& arg) {
  auto promise = Promise2::Promise<T>::Resolved(std::forward<ArgType>(arg));
  return WrappedOnRejectPromise<T>{ promise };
}

template<typename T>
WrappedOnRejectPromise<T> WrappedPromiseResolved<T>::Resolved() {
  return WrappedOnRejectPromise<T>{};
}

template<typename ArgType>
WrappedOnRejectPromise<void> WrappedPromiseResolved<void>::Resolved(ArgType&& arg) {
  return WrappedOnRejectPromise<void>{};
}

WrappedOnRejectPromise<void> WrappedPromiseResolved<void>::Resolved() {
  auto promise = Promise2::Promise<void>::Resolved();
  return WrappedOnRejectPromise<void>{ promise };
}

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
      WrappedOnRejectPromise<bool>::Resolved(truth).then([=](bool fulfilled){ \
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
      WrappedOnRejectPromise<bool>::Rejected(std::make_exception_ptr(UserException())).then([=](bool){ \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, std::bind(passUserException, notifier, std::placeholders::_1), context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from returned task", [](const LTest::SharedCaseEndNotifier& notifier){ \
      constexpr bool truth = true; \
      WrappedOnRejectPromise<bool>::New([=]{ return truth; }, context::New()).then([=](bool fulfilled){ \
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
      WrappedOnRejectPromise<bool>::New([]() -> bool { throw UserException(); }, context::New()).then([=](bool){ \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, std::bind(passUserException, notifier, std::placeholders::_1), context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier){ \
      constexpr bool truth = true; \
      WrappedOnRejectPromise<bool>::New([=]{ return Promise2::Promise<bool>::Resolved(truth); }, context::New()).then([=](bool fulfilled){ \
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
      WrappedOnRejectPromise<bool>::New([]{ \
        return Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())); \
      }, context::New()).then([=](bool fulfilled){ \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, std::bind(passUserException, notifier, std::placeholders::_1), context::New()); \
    }) \
    \
    /* ==> */ \
    .it(#tag"should acquire the fulfilled value from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier){ \
      constexpr bool truth = true; \
      WrappedOnRejectPromise<bool>::New([=](Promise2::PromiseDefer<bool>&& deferred){ \
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
      WrappedOnRejectPromise<bool>::New([](Promise2::PromiseDefer<bool>&& deferred){ \
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
      auto p = WrappedOnRejectPromise<bool>::New([=] { return truth; }, CurrentContext::New());
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
      auto p = WrappedOnRejectPromise<bool>::New([]() -> bool { throw UserException(); }, CurrentContext::New());
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
      auto p = WrappedOnRejectPromise<bool>::New([=](Promise2::PromiseDefer<bool>&& deferred) {
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
      auto p = WrappedOnRejectPromise<bool>::New([](Promise2::PromiseDefer<bool>&& deferred) {
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
      auto p = WrappedOnRejectPromise<bool>::New([=]() {
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
      auto p = WrappedOnRejectPromise<bool>::New([] {
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
      WrappedOnRejectPromise<bool> p;

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
    static constexpr const char *stringValue = "const string";
    static constexpr std::int32_t arrayValues[] = { 2, 3, 4 };

  public:
    static bool validate(const NormalClass&);

  private:
    std::string c1;
    std::vector<std::int32_t> c2;

  protected:
    std::int32_t m1;

  public:
    std::float_t m2;
    std::double_t m3;
    std::int64_t m4;
    std::int8_t *m5;

  public:
    NormalClass();
    NormalClass(const NormalClass& c);

    ~NormalClass() 
    {}
  };

  // virtual methods class
  class VirtualMethodClass {
  private:
    static constexpr const char *stringValue = "virtual const string";
    static constexpr std::int32_t arrayValues[] = { 2, 3, 4 };

  public:
    static bool validate(const VirtualMethodClass&);

  private:
    std::string c1;
    std::vector<std::int32_t> c2;

  protected:
    std::int32_t m1;

  public:
    std::float_t m2;
    std::double_t m3;
    std::int64_t m4;
    std::int8_t *m5;

  public:
    VirtualMethodClass();
    virtual ~VirtualMethodClass() = default;

  public:
    virtual void methodDef() {}
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
    
#if !defined(_MSC_VER)
  // 
  constexpr const char *NormalClass::stringValue;
  constexpr std::int32_t NormalClass::arrayValues[];

  constexpr const char *VirtualMethodClass::stringValue;
  constexpr std::int32_t VirtualMethodClass::arrayValues[];
#endif // _MSC_VER

  bool NormalClass::validate(const NormalClass& c) {
    return c.c1 == stringValue && 
      std::equal(c.c2.begin(), c.c2.end(), arrayValues) &&
      c.m1 == 5 &&
      validateCommonData(c);
  }

  template<typename T, std::uint32_t sizeofArray>
  static T* end(T(&array)[sizeofArray]) {
    return std::decay_t<decltype(array)>(array) + sizeofArray;
  }

  NormalClass::NormalClass()
    : c1{ stringValue }
    , c2{ arrayValues, end(arrayValues) }
    , m1{ 5 } {
    initCommonData(*this);
  }

  NormalClass::NormalClass(const NormalClass& c)
    : c1{ c.c1 }
    , c2{ c.c2 }
    , m1{ c.m1 }
    , m2{ c.m2 }
    , m3{ c.m3 }
    , m4{ c.m4 }
    , m5{ c.m5 } {

  }

  bool VirtualMethodClass::validate(const VirtualMethodClass& c) {
    return c.c1 == stringValue &&
      std::equal(c.c2.begin(), c.c2.end(), arrayValues) &&
      c.m1 == 5 &&
      validateCommonData(c);
  }

  VirtualMethodClass::VirtualMethodClass()
    : c1{ stringValue }
    , c2{ arrayValues, end(arrayValues) }
    , m1{ 5 } {
    initCommonData(*this);
  }

#define DATATEST_INIT(context, tag) \
    spec \
     /* ==> */ \
    .it(#tag"should fulfill trivial type instance correctly", [](const LTest::SharedCaseEndNotifier& notifier){ \
      WrappedOnRejectPromise<TrivialDataType>::New([] { \
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
      WrappedOnRejectPromise<TrivialDataType>::New([](Promise2::PromiseDefer<TrivialDataType>&& deferred) { \
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
      WrappedOnRejectPromise<TrivialDataType>::New([]() { \
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
      WrappedOnRejectPromise<PodDataType>::New([] { \
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
      WrappedOnRejectPromise<PodDataType>::New([](Promise2::PromiseDefer<PodDataType>&& deferred) { \
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
      WrappedOnRejectPromise<PodDataType>::New([]() { \
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
      WrappedOnRejectPromise<StandardLayoutDataType>::New([] { \
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
      WrappedOnRejectPromise<StandardLayoutDataType>::New([](Promise2::PromiseDefer<StandardLayoutDataType>&& deferred) { \
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
      WrappedOnRejectPromise<StandardLayoutDataType>::New([]() { \
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
    }) \
    /* ==> */ \
    .it(#tag"should fulfill normal class instance correctly", [](const LTest::SharedCaseEndNotifier& notifier) { \
      WrappedOnRejectPromise<NormalClass>::New([] { \
        NormalClass data; \
        return data; \
      }, context::New()).then([=](NormalClass data) { \
        if (!NormalClass::validate(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill normal class instance correctly from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      WrappedOnRejectPromise<NormalClass>::New([](Promise2::PromiseDefer<NormalClass>&& deferred) { \
        NormalClass data; \
        deferred.setResult(data); \
      }, context::New()).then([=](NormalClass data) { \
        if (!NormalClass::validate(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill normal class instance correctly from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      WrappedOnRejectPromise<NormalClass>::New([]() { \
        NormalClass data; \
        return Promise2::Promise<NormalClass>::Resolved(data); \
      }, context::New()).then([=](NormalClass data) { \
        if (!NormalClass::validate(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    .it(#tag"should fulfill virtual class instance correctly", [](const LTest::SharedCaseEndNotifier& notifier) { \
      WrappedOnRejectPromise<VirtualMethodClass>::New([] { \
        VirtualMethodClass data; \
        return data; \
      }, context::New()).then([=](VirtualMethodClass data) { \
        if (!VirtualMethodClass::validate(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill virtual class instance correctly from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      WrappedOnRejectPromise<VirtualMethodClass>::New([](Promise2::PromiseDefer<VirtualMethodClass>&& deferred) { \
        VirtualMethodClass data; \
        deferred.setResult(data); \
      }, context::New()).then([=](VirtualMethodClass data) { \
        if (!VirtualMethodClass::validate(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill virtual class instance correctly from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      WrappedOnRejectPromise<VirtualMethodClass>::New([]() { \
        VirtualMethodClass data; \
        return Promise2::Promise<VirtualMethodClass>::Resolved(data); \
      }, context::New()).then([=](VirtualMethodClass data) { \
        if (!VirtualMethodClass::validate(data)) \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        else \
          notifier->done(); \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
      }, context::New()); \
    }) \
    .it(#tag"should fulfill raw pointer correctly", [](const LTest::SharedCaseEndNotifier& notifier) { \
      NormalClass *instancePtr = new NormalClass; \
      WrappedOnRejectPromise<NormalClass *>::New([=] { \
        return instancePtr; \
      }, context::New()).then([=](NormalClass *returned) { \
        if (instancePtr == returned && NormalClass::validate(*returned)) \
          notifier->done(); \
        else \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        /* clean data */ \
        delete instancePtr; \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        /* clean data */ \
        delete instancePtr; \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill raw pointer correctly from deferred promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      NormalClass *instancePtr = new NormalClass; \
      WrappedOnRejectPromise<NormalClass *>::New([=](Promise2::PromiseDefer<NormalClass *>&& deferred) { \
        deferred.setResult(instancePtr); \
      }, context::New()).then([=](NormalClass *returned) { \
        if (instancePtr == returned && NormalClass::validate(*returned)) \
          notifier->done(); \
        else \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        /* clean data */ \
        delete instancePtr; \
      }, [=](std::exception_ptr) { \
        notifier->fail(std::make_exception_ptr(AssertionFailed())); \
        /* clean data */ \
        delete instancePtr; \
      }, context::New()); \
    }) \
    /* ==> */ \
    .it(#tag"should fulfill raw pointer correctly from nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) { \
      NormalClass *instancePtr = new NormalClass; \
      WrappedOnRejectPromise<NormalClass *>::New([=]() { \
        return Promise2::Promise<NormalClass *>::Resolved(instancePtr); \
      }, context::New()).then([=](NormalClass *returned) { \
         if (instancePtr == returned && NormalClass::validate(*returned)) \
           notifier->done(); \
         else \
          notifier->fail(std::make_exception_ptr(AssertionFailed())); \
         /* clean data */ \
         delete instancePtr; \
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
    // string
    // shared_ptr
  }
}

namespace OnRejectReturn {
  template<typename T>
  void init(T& spec) {
    using context = CurrentContext;

    spec
    /* ==> */ 
    .it("should catch the exception thrown from `onReject`", [](const LTest::SharedCaseEndNotifier& notifier){
      Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())).then([=](bool) {
        throw AssertionFailed();
      }, [=](std::exception_ptr e) {
        return Promise2::Promise<void>::Rejected(e);
      }, context::New()).then([=] {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr e) {
        try {
          std::rethrow_exception(e);      
        } catch(const UserException&) {
          notifier->done();
        } catch(...) {
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
        }
        return Promise2::Promise<void>::Resolved();
      }, context::New());
    })
    /* ==> */
    .it("should convey `onReject` returned value downstream via normal promise", [](const LTest::SharedCaseEndNotifier& notifier) {
      constexpr bool truth = true;

      Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())).then([=](bool) {
        return !truth;
      }, [=](std::exception_ptr e) {
        
        try {
           std::rethrow_exception(e);
        } catch(const UserException&) {
          // ignore the exception
          return Promise2::Promise<bool>::New([=] {
            return truth;
          }, context::New());
        } catch(...) {
          return Promise2::Promise<bool>::Rejected(std::current_exception());
        }
      }, context::New()).then([=](bool fulfilled) {
        if (truth != fulfilled) {
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
          return;
        } 
        notifier->done();
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
        return Promise2::Promise<void>::Resolved();
      }, context::New());
    })
    /* ==> */
    .it("should convey `onReject` returned value downstream via deferred promise", [](const LTest::SharedCaseEndNotifier& notifier) {
      constexpr bool truth = true;

      Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())).then([=](bool) {
        return !truth;
      }, [=](std::exception_ptr e) {
        
        try {
           std::rethrow_exception(e);
        } catch(const UserException&) {
          // ignore the exception
          return Promise2::Promise<bool>::New([=](Promise2::PromiseDefer<bool>&& deferred) {
            deferred.setResult(truth);
          }, context::New());
        } catch(...) {
          return Promise2::Promise<bool>::Rejected(std::current_exception());
        }
      }, context::New()).then([=](bool fulfilled) {
        if (truth != fulfilled) {
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
          return;
        } 
        notifier->done();
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
        return Promise2::Promise<void>::Resolved();
      }, context::New());
    })
    /* ==> */
    .it("should convey `onReject` returned value downstream via nesting promise", [](const LTest::SharedCaseEndNotifier& notifier) {
      constexpr bool truth = true;

      Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())).then([=](bool) {
        return !truth;
      }, [=](std::exception_ptr e) {
        
        try {
           std::rethrow_exception(e);
        } catch(const UserException&) {
          // ignore the exception
          return Promise2::Promise<bool>::New([=]() { 
            return Promise2::Promise<bool>::Resolved(truth);
          }, context::New());
        } catch(...) {
          return Promise2::Promise<bool>::Rejected(std::current_exception());
        }
      }, context::New()).then([=](bool fulfilled) {
        if (truth != fulfilled) {
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
          return;
        } 
        notifier->done();
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
        return Promise2::Promise<void>::Resolved();
      }, context::New());
    });
  }
}

namespace ConvertibleArgument {
  template<typename T>
  void init(T& spec) {
    using context = CurrentContext;

    spec
    /* ==> */
    .it("should implicitly convert `int` to `const int&`", [](const LTest::SharedCaseEndNotifier& notifier) {
      constexpr int constant = 1;

      Promise2::Promise<int>::Resolved(constant).then([=](const int& v) {
        if (constant == v)
          notifier->done();
        else
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr e) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
        return Promise2::Promise<void>::Resolved();
      }, context::New());
    });
  }
}

namespace OnRejectImplicitlyResolved {

  template<typename T>
  void init(T& spec) {
    using context = CurrentContext;

    spec
    /* ==> */
    .it("should implicitly resolve with default int value", [](const LTest::SharedCaseEndNotifier& notifier) {
      Promise2::Promise<int>::Rejected(std::make_exception_ptr(UserException())).then([=](int) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
        return 1;
      }, [](std::exception_ptr) {
        // do nothing
      }, context::New()).then([=](int v) {
        const int defaultValue = { int() };
        if (v == defaultValue) 
          notifier->done();
        else 
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, context::New());
    })

    /* ==> */
    .it("should implicitly resolve with default constructed object", [](const LTest::SharedCaseEndNotifier& notifier) {
      static std::atomic_bool called{ false };

      class Class {
      public:
        Class() {
          called = true;
        }

        Class(int) {}
      };
      
      Promise2::Promise<int>::Rejected(std::make_exception_ptr(UserException())).then([=](int) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
        return Class(1);
      }, [](std::exception_ptr) {
        // reset `called` flag
        called = false;
      }, context::New()).then([=](Class) {
        if (called)
          notifier->done();
        else
          notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr) {
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, context::New());
    });
  }
}

TEST_ENTRY(CONTAINER_TYPE,
  SPEC_TFN(SpecFixedValue::init),
  SPEC_TFN(PromiseAPIsBase::init),
  SPEC_TFN(DataValidate::init),
  SPEC_TFN(OnRejectReturn::init),
  SPEC_TFN(ConvertibleArgument::init),
  SPEC_TFN(OnRejectImplicitlyResolved::init));

#endif // PROMISE2_API_CPP
