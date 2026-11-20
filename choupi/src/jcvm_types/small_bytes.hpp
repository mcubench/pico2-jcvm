// Compact owned byte buffer for per-object VM metadata (OOM T9).
//
// Every live JC_Array keeps a persistent tag and, for reference arrays, two
// owner tables. std::vector costs 12 bytes of object space each even when
// empty; this keeps one pointer (4 bytes on the target) to [size16][bytes]
// and offers the subset of the vector interface those members use. Copies
// are deep, so classes holding it keep their implicit copy constructor.

#ifndef _SMALL_BYTES_HPP
#define _SMALL_BYTES_HPP

#include "../exceptions.hpp"

#include <cstdint>
#include <cstring>
#include <utility>

namespace jcvm {

class SmallBytes {
public:
  SmallBytes() noexcept = default;
  SmallBytes(const uint16_t count, const uint8_t fill) {
    if (count == 0) return;
    this->storage = new uint8_t[kHeader + count];
    this->storage[0] = static_cast<uint8_t>(count >> 8);
    this->storage[1] = static_cast<uint8_t>(count);
    std::memset(this->storage + kHeader, fill, count);
  }
  SmallBytes(const uint8_t *first, const uint8_t *last) {
    const auto count = static_cast<uint16_t>(last - first);
    if (count == 0) return;
    this->storage = new uint8_t[kHeader + count];
    this->storage[0] = static_cast<uint8_t>(count >> 8);
    this->storage[1] = static_cast<uint8_t>(count);
    std::memcpy(this->storage + kHeader, first, count);
  }
  SmallBytes(const SmallBytes &other) : SmallBytes(other.begin(), other.end()) {}
  SmallBytes(SmallBytes &&other) noexcept
      : storage(std::exchange(other.storage, nullptr)) {}
  SmallBytes &operator=(const SmallBytes &other) {
    if (this != &other) {
      SmallBytes copy(other);
      std::swap(this->storage, copy.storage);
    }
    return *this;
  }
  SmallBytes &operator=(SmallBytes &&other) noexcept {
    std::swap(this->storage, other.storage);
    return *this;
  }
  ~SmallBytes() { delete[] this->storage; }

  uint16_t size() const noexcept {
    return this->storage == nullptr
               ? 0
               : static_cast<uint16_t>((this->storage[0] << 8) |
                                       this->storage[1]);
  }
  bool empty() const noexcept { return this->storage == nullptr; }
  uint8_t *begin() noexcept { return this->data(); }
  uint8_t *end() noexcept { return this->data() + this->size(); }
  const uint8_t *begin() const noexcept { return this->data(); }
  const uint8_t *end() const noexcept { return this->data() + this->size(); }
  uint8_t &operator[](const uint16_t index) noexcept {
    return this->storage[kHeader + index];
  }
  uint8_t operator[](const uint16_t index) const noexcept {
    return this->storage[kHeader + index];
  }
  uint8_t &at(const uint16_t index) {
    if (index >= this->size()) {
      throw Exceptions::ArrayIndexOutOfBoundsException;
    }
    return (*this)[index];
  }

private:
  static constexpr uint16_t kHeader = 2;
  uint8_t *data() const noexcept {
    return this->storage == nullptr ? nullptr : this->storage + kHeader;
  }
  uint8_t *storage = nullptr;
};

} // namespace jcvm

#endif /* _SMALL_BYTES_HPP */
