#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#define NESTING_PROMISE 1
#define DEFERRED_PROMISE 1
# define USE_DISPATCH 1

#include "Promise.h"
#include "entry.h"
#include "context/ThreadContext_GCD.h"

using MainThreadContext = ThreadContextImpl::GCD::MainThreadContext;

template<typename T>
void init(T& spec) {
	spec 
  /* ==> */ 
  .it("should acquire the fulfilled value", [](const LTest::SharedCaseEndNotifier& n){ 
  	__block auto notifier = n;
    Promise2::Promise<BOOL>::Resolved(YES).then(^(BOOL fulfilled) { 
      if (YES != fulfilled) { 
        notifier->fail(std::make_exception_ptr(AssertionFailed())); 
        return; 
      } 
      notifier->done();
    }, ^(std::exception_ptr e) {
      notifier->fail(std::make_exception_ptr(AssertionFailed())); 
      return Promise2::Promise<void>::Rejected(e);
    }, MainThreadContext::New());
  });
}

TEST_ENTRY(CONTAINER_TYPE,
   SPEC_TFN(init))