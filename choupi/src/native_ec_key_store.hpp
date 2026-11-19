#ifndef JCVM_NATIVE_EC_KEY_STORE_HPP
#define JCVM_NATIVE_EC_KEY_STORE_HPP

#include "heap.hpp"
#include "jc_types/jc_instance.hpp"

#include <cstdint>
#include <vector>

namespace jcvm {
namespace EcKeyStore {

// EC_FP key components, in NativeKey field order: field (p), A, B, G, R
// (order), W (public point), S (private scalar).
constexpr uint16_t kComponentCount = 7;

// Capacity of one component for a key of `bits`. Points are uncompressed.
uint16_t componentCapacity(uint16_t bits, uint16_t component) noexcept;

// Allocate a fresh key's component storage as ONE byte array held by the
// ECFIELD reference: [7 big-endian lengths][component 0]..[component 6], each
// component right-aligned in its capacity. The legacy layout (one array per
// component plus an ECLENGTHS short array) costs 16 more persistent records
// per key; it is still read and written for keys stored by older firmware.
void allocate(Heap &heap, JC_Instance &key, uint16_t bits, bool transient,
              ClearEvent event);

// Stored length of a component (0 when unset).
jshort_t length(JC_Instance &key, uint16_t component);
// Component bytes; throws CryptoException UNINITIALIZED_KEY when unset.
std::vector<uint8_t> read(JC_Instance &key, uint16_t component);
// Replace a component; throws CryptoException ILLEGAL_VALUE when `length` is
// zero or exceeds its capacity.
void write(JC_Instance &key, uint16_t component, const uint8_t *bytes,
           uint16_t length);
// Zero every component and length.
void clear(JC_Instance &key);

} // namespace EcKeyStore
} // namespace jcvm

#endif
