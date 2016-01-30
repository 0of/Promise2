/*
 * Promise2
 *
 * Copyright (c) 2015-2016 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */

#ifndef ENTRY_H
#define ENTRY_H

#include "SpecInitializer.h"
#include "TestSuite.h"

using TestSpec = LTest::SequentialTestSpec;

// template function
#define SPEC_TFN(fn) fn<TestSpec>

#define TEST_ENTRY(...) \
  int main() { \
    auto container = std::make_unique<LTest::SequentialTestRunnableContainer>(); \
    auto spec = std::make_shared<TestSpec>(); \
    \
    SpecInitializer<TestSpec> specInitializer{ *spec }; \
    specInitializer.appendCases(__VA_ARGS__); \
    \
    container->scheduleToRun(spec); \
    container->start(); \
    \
    return 0; \
  }
  
#endif // ENTRY_H





