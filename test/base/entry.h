/*
 * Promise2
 *
 * Copyright (c) 2015-2016 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */

#ifndef ENTRY_H
#define ENTRY_H

#include "TestSuite.h"

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

using TestSpec = LTest::SequentialTestSpec;
using DefaultContainer = LTest::SequentialTestRunnableContainer;

class UserException : public std::exception {};
class AssertionFailed : public std::exception {};

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





