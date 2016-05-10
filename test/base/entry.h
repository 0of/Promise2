/*
 * Promise2
 *
 * Copyright (c) 2015-2016 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */

#ifndef ENTRY_H
#define ENTRY_H

#include <exception>
#include <atomic>

#include "TestSuite.h"

using TestSpec = LTest::SequentialTestSpec;
using DefaultContainer = LTest::SequentialTestRunnableContainer;

class UserException : public std::exception {};
class AssertionFailed : public std::exception {};

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
#else
# define CONTAINER_TYPE DefaultContainer
#endif // __APPLE__

template<typename Spec>
class SpecInitializer {
private:
  std::add_lvalue_reference_t<Spec> _spec;

public:
  explicit SpecInitializer(std::add_lvalue_reference_t<Spec> spec)
    : _spec{ spec }
  {}

public:
  template<typename Functor>
  void appendCases(Functor&& fn) {
    fn(_spec);
  }

  template<typename Any, typename... Rest>
  void appendCases(Any&& any, Rest&&... rest) {
    appendCases(std::forward<Any>(any));
    appendCases(std::forward<Rest>(rest)...);
  }
};

// template function
#define SPEC_TFN(fn) fn<TestSpec>

#define TEST_ENTRY(ContainerType, ...) \
  int main() { \
    auto container = std::make_unique<ContainerType>(); \
    auto spec = std::make_shared<TestSpec>(); \
    \
    SpecInitializer<TestSpec> specInitializer{ *spec }; \
    specInitializer.appendCases(__VA_ARGS__); \
    \
    container->scheduleToRun(spec); \
    container->start(); \
    \
    std::this_thread::sleep_for(5s); \
    \
    return 0; \
  }
  
#endif // ENTRY_H





