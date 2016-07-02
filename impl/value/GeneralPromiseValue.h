/*
 * Promise2
 *
 * Copyright (c) 2015-16 "0of" Magnus
 * Licensed under the MIT license.
 * https://github.com/0of/Promise2/blob/master/LICENSE
 */

#ifndef GENERAL_PROMISE_VALUE_H
#define GENERAL_PROMISE_VALUE_H

#include "PromiseValueBase.h"

namespace Promise2 {
  namespace Details {

    template<typename T>
    class PromiseValue : public PromiseValueBase {
    private:
      std::unique_ptr<T> _valuePointer;

    public:
      template<typename ValueType>
      void setValue(ValueType&& v) {
        if (_assignGuard.test_and_set()) {
          // already assigned or is assigning
          throw std::logic_error("promise duplicated assignments");
        }

        _valuePointer = std::make_unique<T>(std::forward<ValueType>(v));
        _hasAssigned = true;
      }

      template<typename ValueType>
      ValueType getValue() {
        return static_cast<ValueType>(*_valuePointer);
      }
    };

    template<typename T>
    class PromisePointerValue : public PromiseValueBase {
    private:
      // NOT take the ownership
      std::remove_cv_t<T> _value;

    public:
      template<typename ValueType>
      void setValue(const ValueType& v) {
        static_assert(std::is_pointer<ValueType>::value, "given type must be kind of pointer");

        if (_assignGuard.test_and_set()) {
          // already assigned or is assigning
          throw std::logic_error("promise duplicated assignments");
        }

        _value = v;
        _hasAssigned = true;
      }

      template<typename ValueType>
      ValueType getValue() {
        return static_cast<ValueType>(_value);
      }
    };

    template<typename T>
    class PromiseRefValue : public PromiseValueBase {
    private:
      // NOT take the ownership
      std::add_pointer_t<T> _pointerValue;

    public:
      template<typename ValueType>
      void setValue(ValueType&& v) {
        static_assert(std::is_reference<ValueType>::value, "given type must be reference type");

        if (_assignGuard.test_and_set()) {
          // already assigned or is assigning
          throw std::logic_error("promise duplicated assignments");
        }

        // store reference type address
        _pointerValue = &(static_cast<decltype((v))>(v));
        _hasAssigned = true;
      }

      template<typename ValueType>
      ValueType getValue() {
        static_assert(std::is_reference<ValueType>::value, "given type must be reference type");
        return static_cast<ValueType>(*_pointerValue);
      }
    };
  }
}

#endif // GENERAL_PROMISE_VALUE_H

