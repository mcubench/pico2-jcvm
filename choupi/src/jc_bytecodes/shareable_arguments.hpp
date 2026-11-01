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
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
*/

#ifndef JCVM_SHAREABLE_ARGUMENTS_HPP
#define JCVM_SHAREABLE_ARGUMENTS_HPP

#include "../interpretor.hpp"
#include "../stack.hpp"
#include <array>

namespace jcvm {

// Convert a CAP Descriptor method signature and the caller's operand words
// into initial locals for a nested Shareable method call. Caller words remain
// untouched. Null and JCRE-global references can cross contexts because they
// are not owned by either applet heap. Applet-local references remain rejected
// until ownership can be translated safely.
inline bool buildShareableInitialLocals(
    const uint8_t *signature_nibbles, const uint8_t signature_nibble_count,
    const uint8_t argument_word_count, const jref_t server_reference,
    Stack &caller_stack,
    std::array<InterpretorInitialLocal, 255> &initial_locals,
    uint8_t &initial_local_count) {
  if (signature_nibbles == nullptr || signature_nibble_count == 0 ||
      argument_word_count == 0) {
    return false;
  }

  initial_local_count = 1;
  initial_locals[0] = {InterpretorLocalType::Reference,
                       server_reference.compact()};
  uint16_t signature_cursor = 0;
  uint16_t argument_word_index = 1; // word zero is the receiver.
  uint16_t expected_argument_words = 1;
  bool found_return_type = false;
  const auto caller_word = [&caller_stack, argument_word_count](
                               const uint16_t position) {
    return caller_stack
        .get_Pushed_Element(
            static_cast<uint16_t>(argument_word_count - position))
        .compact();
  };

  while (signature_cursor < signature_nibble_count) {
    const uint8_t type = signature_nibbles[signature_cursor++];
    if (type == 0x06 || type == 0x0e) {
      if (signature_nibble_count - signature_cursor < 4) return false;
      signature_cursor = static_cast<uint16_t>(signature_cursor + 4);
    }
    if (signature_cursor == signature_nibble_count) {
      found_return_type = true;
      break;
    }
    if (initial_local_count == initial_locals.size() ||
        argument_word_index >= argument_word_count) {
      return false;
    }

    switch (type) {
    case 0x02: // boolean
    case 0x03: // byte
      initial_locals[initial_local_count++] = {
          InterpretorLocalType::Byte,
          static_cast<uint32_t>(static_cast<int8_t>(
              caller_word(argument_word_index)))};
      ++argument_word_index;
      ++expected_argument_words;
      break;
    case 0x04: // short
      initial_locals[initial_local_count++] = {
          InterpretorLocalType::Short,
          static_cast<uint32_t>(static_cast<uint16_t>(
              caller_word(argument_word_index)))};
      ++argument_word_index;
      ++expected_argument_words;
      break;
    case 0x05: { // int occupies two stack words and two local slots.
#ifdef JCVM_INT_SUPPORTED
      if (static_cast<uint16_t>(argument_word_index + 1) >=
          argument_word_count) {
        return false;
      }
      const uint16_t low_word = caller_word(argument_word_index);
      const uint16_t high_word =
          caller_word(static_cast<uint16_t>(argument_word_index + 1));
      initial_locals[initial_local_count++] = {
          InterpretorLocalType::Int,
          static_cast<uint32_t>(SHORTS_TO_INT(high_word, low_word))};
      argument_word_index = static_cast<uint16_t>(argument_word_index + 2);
      expected_argument_words =
          static_cast<uint16_t>(expected_argument_words + 2);
#else
      return false;
#endif
      break;
    }
    case 0x06: // object reference
    case 0x0a: // boolean[]
    case 0x0b: // byte[]
    case 0x0c: // short[]
    case 0x0d: // int[]
    case 0x0e: { // reference[]
      const jref_t reference(caller_word(argument_word_index));
      if (!reference.isNullPointer() && !reference.isGlobalArray() &&
          !reference.isGlobalInstance()) {
        return false;
      }
      const bool expected_array = type != 0x06;
      if (!reference.isNullPointer() && reference.isArray() != expected_array) {
        return false;
      }
      initial_locals[initial_local_count++] = {
          InterpretorLocalType::Reference, reference.compact()};
      ++argument_word_index;
      ++expected_argument_words;
      break;
    }
    default:
      return false;
    }
  }

  return found_return_type && expected_argument_words == argument_word_count &&
         argument_word_index == argument_word_count;
}

} // namespace jcvm

#endif // JCVM_SHAREABLE_ARGUMENTS_HPP
