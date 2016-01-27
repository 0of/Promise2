#include "TestSuite.h"

void initSpec(LTest::SequentialTestSpec& spec) {
  spec.it("should be all ok", []{});
}

int main() {
   // init container
  auto container = std::make_unique<LTest::SequentialTestRunnableContainer>();

  // alloc spec
  auto spec = std::make_shared<LTest::SequentialTestSpec>();

  // init spec
  initSpec(*spec);

  // assign the spec as first runnable to run
  container->scheduleToRun(spec);

  // run the container
  container->start();

  return 0;
}