/*
** The MIT License (MIT)
**
** Copyright (c) 2020, National Cybersecurity Agency of France (ANSSI)
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Author:
**   - Guillaume Bouffard <guillaume.bouffard@ssi.gouv.fr>
*/

#ifndef _JCVMARRAY_HPP
#define _JCVMARRAY_HPP

#include "../jc_config.h"
#include "../types.hpp"

#include <type_traits>

#ifdef JCVM_ARRAY_SIZE_CHECK
#include "../exceptions.hpp"
#endif /* JCVM_ARRAY_SIZE_CHECK */

namespace jcvm {

template <typename T> class JCVMArray {
private:
  uint16_t length;
  bool allocated_data;
  T *array;

  void release() noexcept {
    if (this->allocated_data) {
      delete[] this->array;
    }
    this->array = nullptr;
    this->length = 0;
    this->allocated_data = false;
  }

public:
  /*
   * Default class constructor.
   */
  JCVMArray(const uint16_t length) noexcept {
    this->length = length;
    this->array = nullptr;
    this->allocated_data = false;

    if (this->length > 0) {
      this->array = new T[length]{};
      this->allocated_data = true;
    } else {
      this->allocated_data = false;
    }
  }

  JCVMArray(const uint16_t length, T *data) noexcept {
    this->length = length;
    this->array = data;
    this->allocated_data = false;
  }

  JCVMArray(const uint16_t length, T *data, const bool take_ownership) noexcept {
    this->length = length;
    this->array = data;
    this->allocated_data = take_ownership;
  }

  JCVMArray(const JCVMArray &other) {
    this->length = other.length;
    this->array = nullptr;
    this->allocated_data = false;

    if constexpr (std::is_const_v<T>) {
      this->array = other.array;
    } else if (this->length > 0) {
      this->array = new T[this->length];
      this->allocated_data = true;
      for (uint16_t index = 0; index < this->length; ++index) {
        this->array[index] = other.array[index];
      }
    }
  }

  JCVMArray(JCVMArray &&other) noexcept
      : length(other.length), allocated_data(other.allocated_data),
        array(other.array) {
    other.length = 0;
    other.allocated_data = false;
    other.array = nullptr;
  }

  JCVMArray &operator=(const JCVMArray &other) {
    if (this == &other) {
      return *this;
    }

    this->release();
    this->length = other.length;

    if constexpr (std::is_const_v<T>) {
      this->array = other.array;
    } else if (this->length > 0) {
      this->array = new T[this->length];
      this->allocated_data = true;
      for (uint16_t index = 0; index < this->length; ++index) {
        this->array[index] = other.array[index];
      }
    }
    return *this;
  }

  JCVMArray &operator=(JCVMArray &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    this->release();
    this->length = other.length;
    this->allocated_data = other.allocated_data;
    this->array = other.array;
    other.length = 0;
    other.allocated_data = false;
    other.array = nullptr;
    return *this;
  }

  /*
   * Default class destructor.
   */
  ~JCVMArray() noexcept {
    this->release();
  }

  /*
   * Access specified element with bounds checking.
   *
   * @param[index] Get the indexed element.
   * @return Get the indexed element.
   */
  T &operator[](uint16_t index)
#ifndef JCVM_ARRAY_SIZE_CHECK
      noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
  {
    return this->at(index);
  }

  /*
   * Access specified element with bounds checking.
   *
   * @param[index] Get the indexed element.
   * @return Get the indexed element.
   */
  constexpr T &operator[](uint16_t index) const
#ifndef JCVM_ARRAY_SIZE_CHECK
      noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
  {
    return this->at(index);
  }

  /*
   * Access specified element with bounds checking.
   *
   * @param[index] Get the indexed element.
   * @return Get the indexed element.
   */
  T &at(uint16_t index)
#ifndef JCVM_ARRAY_SIZE_CHECK
      noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
  {
#ifdef JCVM_ARRAY_SIZE_CHECK

    if (index >= this->length) {
      throw Exceptions::IndexOutOfBoundsException;
    }

#endif /* JCVM_ARRAY_SIZE_CHECK */
    return this->array[index];
  }

  /*
   * Access specified element with bounds checking.
   *
   * @param[index] Get the indexed element.
   * @return Get the indexed element.
   */
  constexpr T &at(uint16_t index) const
#ifndef JCVM_ARRAY_SIZE_CHECK
      noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
  {
#ifdef JCVM_ARRAY_SIZE_CHECK

    if (index >= this->length) {
      throw Exceptions::IndexOutOfBoundsException;
    }

#endif /* JCVM_ARRAY_SIZE_CHECK */
    return this->array[index];
  }

  /*
   * Direct access to the underlying array.
   *
   * @return Pointer to the underlying array.
   */
  T *data() noexcept { return this->array; }

  /*
   * Direct access to the underlying array.
   *
   * @return Constant pointer to the underlying array.
   */
  const T *data() const noexcept { return this->array; }

  /*
   * Returns the maximum possible number of elements
   *
   * @return the maximum possible number of elements
   */
  uint16_t size() const noexcept { return this->length; }

  /*
   * Assigns the given value value to all elements in the container.
   *
   * @param[value] the value used to fill the array.
   */
  void fill(const T &value) noexcept {
    for (int index = 0; index < this->size(); ++index) {
      this->array[index] = value;
    }
  }
};

} // namespace jcvm

#endif /* _JCVMARRAY_HPP */
