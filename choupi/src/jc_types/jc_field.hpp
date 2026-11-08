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

#ifndef _JC_FIELD_HPP
#define _JC_FIELD_HPP

#include "../types.hpp"

namespace jcvm {

enum FieldType : uint8_t {
  FIELD_TYPE_BYTE = (uint8_t)0,
  FIELD_TYPE_BOOLEAN = (uint8_t)1,
  FIELD_TYPE_SHORT = (uint8_t)2,
#ifdef JCVM_INT_SUPPORTED
  FIELD_TYPE_INT = (uint8_t)3,
#endif /* JCVM_INT_SUPPORTED */
  FIELD_TYPE_OBJECT = (uint8_t)4,
  FIELD_TYPE_GLOBAL_AID_OBJECT = (uint8_t)5,
  // Internal persistent marker; never exposed as a Java Card field type.
  FIELD_TYPE_SHAREABLE_PROXY_OBJECT = (uint8_t)6,
  // Internal wrapper for a static reference whose value belongs to another
  // package. The record contains the owner package and a target persistent tag.
  FIELD_TYPE_FOREIGN_REFERENCE = (uint8_t)8,
  // The trailing word of an in-memory Java Card int field. It is never
  // serialized as a separate field record.
  FIELD_TYPE_INT_LOW = (uint8_t)7,

  FIELD_TYPE_ARRAY_BYTE = (uint8_t)((1 << 7) | FIELD_TYPE_BYTE),
  FIELD_TYPE_ARRAY_BOOLEAN = (uint8_t)((1 << 7) | FIELD_TYPE_BOOLEAN),
  FIELD_TYPE_ARRAY_SHORT = (uint8_t)((1 << 7) | FIELD_TYPE_SHORT),
#ifdef JCVM_INT_SUPPORTED
  FIELD_TYPE_ARRAY_INT = (uint8_t)((1 << 7) | FIELD_TYPE_INT),
#endif /* JCVM_INT_SUPPORTED */
  FIELD_TYPE_ARRAY_OBJECT = (uint8_t)((1 << 7) | FIELD_TYPE_OBJECT),

  FIELD_TYPE_TRANSIENT_ARRAY_BYTE = (uint8_t)((1 << 6) | FIELD_TYPE_ARRAY_BYTE),
  FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN =
      (uint8_t)((1 << 6) | FIELD_TYPE_ARRAY_BOOLEAN),
  FIELD_TYPE_TRANSIENT_ARRAY_SHORT =
      (uint8_t)((1 << 6) | FIELD_TYPE_ARRAY_SHORT),
#ifdef JCVM_INT_SUPPORTED
  FIELD_TYPE_TRANSIENT_ARRAY_INT = (uint8_t)((1 << 6) | FIELD_TYPE_ARRAY_INT),
#endif /* JCVM_INT_SUPPORTED */
  FIELD_TYPE_TRANSIENT_ARRAY_OBJECT =
      (uint8_t)((1 << 6) | FIELD_TYPE_ARRAY_OBJECT),

  FIELD_TYPE_UNINITIALIZED = (uint8_t)(-1),

}; // namespace jcvm

struct jc_field_t {
  FieldType type;
  // Package provenance for volatile reference-valued instance fields. The
  // current-package marker keeps the common case compact and compatible with
  // existing field initializers; foreign package IDs are retained explicitly.
  uint8_t reference_owner = 0xff;
  jword_t value;
};
static_assert(sizeof(jc_field_t) == sizeof(jword_t) + 2,
              "reference ownership metadata must not enlarge field slots");

} // namespace jcvm

#endif /* _JC_FIELD_HPP */
