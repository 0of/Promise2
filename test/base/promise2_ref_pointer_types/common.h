#ifndef COMMON_H
#define COMMON_H

#include <atomic>

#include "entry.h"
#include "Promise.h"

// type alias
using AsyncTestCaseNotifier = LTest::SharedCaseEndNotifier;

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

#endif // COMMON_H
