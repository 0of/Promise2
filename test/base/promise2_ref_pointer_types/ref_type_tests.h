#ifndef REF_TYPE_TESTS_H
#define REF_TYPE_TESTS_H

#include "common.h"

namespace RefTypeBase {
  template<typename T>
  void init(T& spec) {
    spec      
    /* ==> */
    .it("should same primitive type ref value address", [](const AsyncTestCaseNotifier& notifier){
      constexpr const int value = 5;

      auto valuePointer = new int{ value };
      Promise2::Promise<int&>::Resolved(*valuePointer).then([=](int& refValue){
        std::unique_ptr<int> p { &refValue };
        if (p.get() == valuePointer && refValue == value) {
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

#endif // REF_TYPE_TESTS_H
