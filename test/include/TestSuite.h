/*
* Promise2
*
* Copyright (c) 2015 "0of" Magnus
* Licensed under the MIT license.
* https://github.com/0of/Promise2/blob/master/LICENSE
*/
#include <vector>
#include <iostream>

namespace Promise2 {
	namespace TestSuite {
		
		class TestRunable {
		public:
			virtual ~TestRunable() = default;

		public:
			virtual void run() = 0;
		};

		template<typename String, typename Callable>
		class TestCase : TestRunable {
		private:
			Callable _callable;
			String _caseDescription;

		public:
			TestCase(String&& desc, Callable callable)
				: _callable{ std::forward<Callable>(callable) }
				, _caseDescription{ std::forward<Callable>(desc) }
			{}

		public:
			void run() override {
				// print the description
				std::cout << _caseDescription << std::endl;
				_callable();
			}
		}

		using RunableChain = std::std::vector<std::shared_ptr<TestRunable>>;

		class TestSpec {
		private:
			std::string _description;
			RunableChain _runableChain;

		public:
			template<typename String, typename Callable>
			void it(Sring&& should, Callable&& callable) {
				_runableChain.push_back(new TestCase<String, Callable>(should, callable));
			}

		public:
			void start() {
				for (auto& eachCase : _runableChain) {
					eachCase->run();
				}
			}
		};
	} // TestSuite
} // Promise2

