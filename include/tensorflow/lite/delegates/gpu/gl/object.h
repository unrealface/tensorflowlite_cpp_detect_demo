/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_LITE_DELEGATES_GPU_GL_OBJECT_H_
#define TENSORFLOW_LITE_DELEGATES_GPU_GL_OBJECT_H_

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "absl/types/variant.h"
#include "tensorflow/lite/delegates/gpu/common/data_type.h"
#include "tensorflow/lite/delegates/gpu/common/shape.h"
#include "tensorflow/lite/delegates/gpu/common/types.h"
#include "tensorflow/lite/delegates/gpu/common/util.h"

namespace tflite {
namespace gpu {
namespace gl {

enum class AccessType {
  READ,
  WRITE,
  READ_WRITE,
};

using ObjectData = std::vector<uint8_t>;

// Generic identifier to be used to lookup an object.
using ObjectRef = uint32_t;

constexpr ObjectRef kInvalidObjectRef = ~0;

enum class ObjectType : int {
  UNKNOWN = 0,
  TEXTURE = 1,
  BUFFER = 2,
};

using ObjectSize = absl::variant<uint32_t, uint2, uint3>;

// An object represents a reference to or pre-defined constant OpenGL Buffer or
// Texture. NodeShader is supposed to set all fields but leave binding = 0
// that will be set later by a compiler.
struct Object {
  AccessType access;

  DataType data_type;

  ObjectType object_type;

  // OpenGL-specific binding information
  uint32_t binding;

  // Indicates size of 1D, 2D or 3D object in elements, where single element
  // consists of 4 values.
  ObjectSize size;

  absl::variant<ObjectData, ObjectRef> object;
};

// @return true if object is a reference.
inline bool IsRef(const Object& object) {
  return !absl::get_if<ObjectData>(&object.object);
}

inline ObjectRef GetRef(const Object& object) {
  auto ref = absl::get_if<ObjectRef>(&object.object);
  return ref ? *ref : kInvalidObjectRef;
}

inline const ObjectData* GetData(const Object& object) {
  return absl::get_if<ObjectData>(&object.object);
}

inline size_t ByteSizeOf(const Object& object);

// @return object that references an object created externally.
template <typename SizeT>
inline Object MakeObjectRef(ObjectRef unique_id, const SizeT& size,
                            AccessType access_type) {
  return Object{access_type, DataType::FLOAT32, ObjectType::UNKNOWN, 0,
                size,        unique_id};
}

namespace internal_object {

template <typename T>
std::vector<uint8_t> ToBytesVector(const std::vector<T>& data,
                                   size_t alignment) {
  std::vector<uint8_t> t(AlignByN(data.size() * sizeof(T), alignment));
  std::memcpy(t.data(), data.data(), data.size() * sizeof(T));
  return t;
}

struct ObjectSizer {
  size_t operator()(const uint3& size) const {
    return size.x * size.y * size.z;
  }

  size_t operator()(const uint2& size) const { return size.x * size.y; }

  size_t operator()(uint32_t size) const { return size; }
};

}  // namespace internal_object

inline size_t NumElements(const ObjectSize& size) {
  return absl::visit(internal_object::ObjectSizer{}, size);
}

inline size_t ByteSizeOf(const Object& object) {
  return SizeOf(object.data_type) * /* vec4 */ 4 * NumElements(object.size);
}

template <typename SizeT>
Object MakeReadonlyObject(const SizeT& size, const std::vector<float>& data) {
  return Object{AccessType::READ,
                DataType::FLOAT32,
                ObjectType::UNKNOWN,
                0,
                size,
                internal_object::ToBytesVector(data, 16)};
}

template <typename SizeT>
Object MakeReadonlyTexture(const SizeT& size, const std::vector<float>& data) {
  return Object{AccessType::READ,
                DataType::FLOAT32,
                ObjectType::TEXTURE,
                0,
                size,
                internal_object::ToBytesVector(data, 16)};
}

template <typename SizeT>
Object MakeReadonlyBuffer(const SizeT& size, const std::vector<float>& data) {
  return Object{AccessType::READ,
                DataType::FLOAT32,
                ObjectType::BUFFER,
                0,
                size,
                internal_object::ToBytesVector(data, 16)};
}

inline Object MakeReadonlyObject(const std::vector<float>& data) {
  return MakeReadonlyObject(IntegralDivideRoundUp(data.size(), 4U), data);
}

inline Object MakeReadonlyTexture(const std::vector<float>& data) {
  return MakeReadonlyTexture(IntegralDivideRoundUp(data.size(), 4U), data);
}

inline Object MakeReadonlyBuffer(const std::vector<float>& data) {
  return MakeReadonlyBuffer(IntegralDivideRoundUp(data.size(), 4U), data);
}

// TODO(akulik): find better place for functions below.

inline uint3 GetPHWC4Size(const BHWC& shape) {
  uint3 size;
  size.x = shape.w;
  size.y = shape.h;
  size.z = shape.b * IntegralDivideRoundUp(shape.c, 4);
  return size;
}

inline Object MakePHWC4Ref(uint32_t global_id, const BHWC& shape) {
  return MakeObjectRef(global_id, GetPHWC4Size(shape), AccessType::READ_WRITE);
}

}  // namespace gl
}  // namespace gpu
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_GPU_GL_OBJECT_H_
