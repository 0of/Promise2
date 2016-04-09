#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#define NESTING_PROMISE 1
#define DEFERRED_PROMISE 1
# define USE_DISPATCH 1

#include "Promise.h"
#include "entry.h"
#include "context/ThreadContext_GCD.h"
#indf
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

#define CONTAINER_TYPE GCDContainer

class UserException : public std::exception {};
class AssertionFailed : public std::exception {};

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