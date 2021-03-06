// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "draco/compression/mesh/mesh_sequential_decoder.h"

#include "draco/compression/attributes/linear_sequencer.h"
#include "draco/compression/attributes/sequential_attribute_decoders_controller.h"
#include "draco/core/symbol_decoding.h"

namespace draco {

MeshSequentialDecoder::MeshSequentialDecoder() {}

bool MeshSequentialDecoder::DecodeConnectivity() {
  int32_t num_faces;
  if (!buffer()->Decode(&num_faces) || num_faces < 0)
    return false;
  int32_t num_points;
  if (!buffer()->Decode(&num_points) || num_points < 0)
    return false;
  uint8_t connectivity_method;
  if (!buffer()->Decode(&connectivity_method))
    return false;
  if (connectivity_method == 0) {
    if (!DecodeAndDecompressIndices(num_faces))
      return false;
  } else {
    if (num_points < 256) {
      // Decode indices as uint8_t.
      for (int i = 0; i < num_faces; ++i) {
        Mesh::Face face;
        for (int j = 0; j < 3; ++j) {
          uint8_t val;
          if (!buffer()->Decode(&val))
            return false;
          face[j] = val;
        }
        mesh()->AddFace(face);
      }
    } else if (num_points < (1 << 16)) {
      // Decode indices as uint16_t.
      for (int i = 0; i < num_faces; ++i) {
        Mesh::Face face;
        for (int j = 0; j < 3; ++j) {
          uint16_t val;
          if (!buffer()->Decode(&val))
            return false;
          face[j] = val;
        }
        mesh()->AddFace(face);
      }
    } else {
      // Decode faces as uint32_t (default).
      for (int i = 0; i < num_faces; ++i) {
        Mesh::Face face;
        for (int j = 0; j < 3; ++j) {
          uint32_t val;
          if (!buffer()->Decode(&val))
            return false;
          face[j] = val;
        }
        mesh()->AddFace(face);
      }
    }
  }
  point_cloud()->set_num_points(num_points);
  return true;
}

bool MeshSequentialDecoder::CreateAttributesDecoder(int32_t att_decoder_id) {
  // Always create the basic attribute decoder.
  SetAttributesDecoder(
      att_decoder_id,
      std::unique_ptr<AttributesDecoder>(
          new SequentialAttributeDecodersController(
              std::unique_ptr<PointsSequencer>(
                  new LinearSequencer(point_cloud()->num_points())))));
  return true;
}

bool MeshSequentialDecoder::DecodeAndDecompressIndices(int32_t num_faces) {
  // Get decoded indices differences that were encoded with an entropy code.
  std::vector<uint32_t> indices_buffer(num_faces * 3);
  if (!DecodeSymbols(num_faces * 3, 1, buffer(), indices_buffer.data()))
    return false;
  // Reconstruct the indices from the differences.
  // See MeshSequentialEncoder::CompressAndEncodeIndices() for more details.
  int32_t last_index_value = 0;
  int vertex_index = 0;
  for (int i = 0; i < num_faces; ++i) {
    Mesh::Face face;
    for (int j = 0; j < 3; ++j) {
      const uint32_t encoded_val = indices_buffer[vertex_index++];
      int32_t index_diff = (encoded_val >> 1);
      if (encoded_val & 1)
        index_diff = -index_diff;
      const int32_t index_value = index_diff + last_index_value;
      face[j] = index_value;
      last_index_value = index_value;
    }
    mesh()->AddFace(face);
  }
  return true;
}

}  // namespace draco
