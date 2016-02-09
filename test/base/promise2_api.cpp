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
/*
 * testing APIs
 */
#include "Promise.h"
#include "entry.h"

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

  template<typename T>
  void init(T& spec) {
    spec
    // ==>
    .it("should acquire the fulfilled value", []{
      constexpr bool truth = true;
      Promise2::Promise<bool>::Resolved(truth).then([=](bool fulfilled){
        if (truth != fulfilled)
          throw AssertionFailed();
      }, [](std::exception_ptr) {
        throw AssertionFailed();
      }, CurrentContext::New());
    })

    // ==>
    .it("should transfer the exception downstream", []{
      Promise2::Promise<bool>::Rejected(std::make_exception_ptr(UserException())).then([](bool){
        throw AssertionFailed();
      }, passUserException, CurrentContext::New());
    })

    // ==>
    .it("should acquire the fulfilled value from returned task", []{
      constexpr bool truth = true;
      Promise2::Promise<bool>::New([=]{ return truth; }, CurrentContext::New()).then([=](bool fulfilled){
        if (truth != fulfilled)
            throw AssertionFailed();
      }, [](std::exception_ptr) {
          throw AssertionFailed();
      }, CurrentContext::New());
    })
      
    // ==>
    .it("should transfer the exception downstream from returned task", []{
      Promise2::Promise<bool>::New([]() -> bool { throw UserException(); }, CurrentContext::New()).then([](bool){
        throw AssertionFailed();
      }, passUserException, CurrentContext::New());
    })
    
    // ==>
    .it("should acquire the fulfilled value from nesting promise", []{
      constexpr bool truth = true;
      Promise2::Promise<bool>::New([=]{ return Promise2::Promise<bool>::Resolved(truth); }, CurrentContext::New()).then([=](bool fulfilled){
        if (truth != fulfilled)
            throw AssertionFailed();
      }, [](std::exception_ptr) {
          throw AssertionFailed();
      }, CurrentContext::New());
    });
  // end of the init spec
  }
} // SpecFixedValue

TEST_ENTRY(SPEC_TFN(SpecFixedValue::init))

#endif // PROMISE2_API_CPP
