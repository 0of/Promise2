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

namespace RecursionAPIsBase {

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
    });
  }

} // RecursionAPIsBase

TEST_ENTRY(CONTAINER_TYPE,
  SPEC_TFN(RecursionAPIsBase::init));
