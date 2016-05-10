/*
* Promise2
*
* Copyright (c) 2015-16 "0of" Magnus
* Licensed under the MIT license.
* https://github.com/0of/Promise2/blob/master/LICENSE
*/
#ifndef POINTER_TYPE_TESTS_H
#define POINTER_TYPE_TESTS_H

#include <atomic>

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

// type alias
using AsyncTestCaseNotifier = LTest::SharedCaseEndNotifier;

namespace PointerTypeBase {
  template<typename T>
  void init(T& spec) {
  	spec      
  	/* ==> */
    .it("should same primitive type value address", [](const AsyncTestCaseNotifier& notifier){
    	constexpr const int value = 5;

    	auto valuePointer = new int{ value };
    	Promise2::Promise<int *>::Resolved(valuePointer).then([=](int *pointer){
        std::unique_ptr<int> p { valuePointer };
        if (p.get() == pointer && *pointer == value) {
        	notifier->done();
        	return;
        } 

        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, [=](std::exception_ptr) {
        std::unique_ptr<int> p { valuePointer };
        notifier->fail(std::make_exception_ptr(AssertionFailed()));
      }, CurrentContext::New());
    });
  }
}

#endif // POINTER_TYPE_TESTS_H
