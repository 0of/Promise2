/*
 * Promise2
 *
 * Copyright (c) 2015-16 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */

#ifndef PROMISE_VALUE_BASE_H
#define PROMISE_VALUE_BASE_H

#include <atomic>
#include <exception>

namespace Promise2 {
  namespace Details {
    class PromiseValueBase {
    protected:
      std::exception_ptr _exception;

      std::atomic_flag _assignGuard;
      std::atomic_bool _hasAssigned;

    protected:
      PromiseValueBase()
        : _exception{ nullptr }
        , _assignGuard{ ATOMIC_FLAG_INIT }
        , _hasAssigned{ false }
      {}

    public:
      void setException(std::exception_ptr e) {
        if (_assignGuard.test_and_set()) {
          // already assigned or is assigning
          throw std::logic_error("promise duplicated assignments");
        }

        _exception = e;
        _hasAssigned = true;
      }

    public:
      //
      // access guard must be invoked before directly access the `value` 
      // check the assigned flag and make sure no exception has been thrown
      //
      void accessGuard() {
        if (!_hasAssigned) {
          throw std::logic_error("promise invalid state");
        }

        if (_exception) {
          std::rethrow_exception(_exception);
        }
      }

    public:
      std::exception_ptr fetchException() const {
        return _exception;
      }

    public:
      bool hasAssigned() const {
        return _hasAssigned;
      }

      bool isExceptionCase() const {
        return nullptr != _exception;
      }

    private:
      PromiseValueBase(const PromiseValueBase&) = delete;
      PromiseValueBase(const PromiseValueBase&&) = delete;
    };    
  }
}

#endif // PROMISE_VALUE_BASE_H
