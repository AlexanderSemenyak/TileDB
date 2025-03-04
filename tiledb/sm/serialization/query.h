/**
 * @file   query.h
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2018-2022 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * This file declares serialization for the Query class
 */

#ifndef TILEDB_SERIALIZATION_QUERY_H
#define TILEDB_SERIALIZATION_QUERY_H

#include <unordered_map>

#include "tiledb/common/status.h"
#include "tiledb/common/thread_pool.h"
#include "tiledb/sm/query/query_condition.h"
#include "tiledb/sm/storage_manager/storage_manager_declaration.h"
#include "tiledb/sm/subarray/subarray.h"

#ifdef TILEDB_SERIALIZATION
#include "tiledb/sm/serialization/tiledb-rest.capnp.h"
#endif

using namespace tiledb::common;

namespace tiledb {
namespace sm {

class Array;
class Buffer;
class BufferList;
class Query;
class GlobalOrderWriter;
class UnorderedWriter;
class OrderedDimLabelReader;

enum class SerializationType : uint8_t;

namespace serialization {

/**
 * Contains state related to copying data into user's query buffers for an
 * attribute.
 */
struct QueryBufferCopyState {
  /** Accumulated number of bytes copied into user's offset buffer. */
  uint64_t offset_size;

  /** Accumulated number of bytes copied into user's data buffer. */
  uint64_t data_size;

  /** Accumulated number of bytes copied into user's validity buffer. */
  uint64_t validity_size;

  /** Track if the last query added the extra offset. If so we avoid the first
   * 0th offset on the n+1 query. */
  bool last_query_added_extra_offset;

  /** Constructor. */
  QueryBufferCopyState()
      : offset_size(0)
      , data_size(0)
      , validity_size(0)
      , last_query_added_extra_offset(false) {
  }

  /** Copy constructor. */
  QueryBufferCopyState(const QueryBufferCopyState& rhs)
      : offset_size(rhs.offset_size)
      , data_size(rhs.data_size)
      , validity_size(rhs.validity_size)
      , last_query_added_extra_offset(rhs.last_query_added_extra_offset) {
  }

  /** Move constructor. */
  QueryBufferCopyState(QueryBufferCopyState&& rhs)
      : offset_size(rhs.offset_size)
      , data_size(rhs.data_size)
      , validity_size(rhs.validity_size)
      , last_query_added_extra_offset(rhs.last_query_added_extra_offset) {
  }

  /** Destructor. */
  ~QueryBufferCopyState() {
  }

  /** Assignment operator. */
  QueryBufferCopyState& operator=(const QueryBufferCopyState& rhs) {
    offset_size = rhs.offset_size;
    data_size = rhs.data_size;
    validity_size = rhs.validity_size;
    last_query_added_extra_offset = rhs.last_query_added_extra_offset;
    return *this;
  }

  /** Move-assignment operator. */
  QueryBufferCopyState& operator=(QueryBufferCopyState&& rhs) {
    offset_size = rhs.offset_size;
    data_size = rhs.data_size;
    validity_size = rhs.validity_size;
    last_query_added_extra_offset = rhs.last_query_added_extra_offset;
    return *this;
  }
};

/** Maps a buffer name to an associated QueryBufferCopyState. */
using CopyState =
    std::unordered_map<std::string, serialization::QueryBufferCopyState>;

/**
 * Deserialize an array from a buffer containing a serialized query
 *
 * @param serialized_buffer Buffer containing serialized query
 * @param serialize_type Serialization type of serialized query
 * @param array Array object to deserialize into
 */
Status array_from_query_deserialize(
    const Buffer& serialized_buffer,
    SerializationType serialize_type,
    Array& array,
    StorageManager* storage_manager);

/**
 * Serialize a query
 *
 * @param query Query to serialize
 * @param serialize_type format to serialize to
 * @param clientside Whether serialization should be performed from a client
 *      or server perspective
 * @param serialized_buffer Buffer to store serialized query
 */
Status query_serialize(
    Query* query,
    SerializationType serialize_type,
    bool clientside,
    BufferList* serialized_buffer);

/**
 * Deserialize a query. This takes a buffer containing serialized query
 * information (read state, attribute data, query status, etc) and updates the
 * given query to be equivalent to the serialized query.
 *
 * @param serialized_buffer Buffer containing serialized query
 * @param serialize_type Serialization type of serialized query
 * @param clientside Whether deserialization should be performed from a client
 *      or server perspective
 * @param copy_state Map of copy state per attribute. If this is null, the
 *      query's buffer sizes are updated directly. If it is not null, the buffer
 *      sizes are not modified but the entries in the map are.
 * @param query Query to deserialize into
 */
Status query_deserialize(
    const Buffer& serialized_buffer,
    SerializationType serialize_type,
    bool clientside,
    CopyState* copy_state,
    Query* query,
    ThreadPool* compute_tp);

/**
 * Serialize an estimated result size map for all fields from a query object
 *
 * @param query Query to get est
 * @param serialize_type
 * @param clientside
 * @param serialized_buffer
 * @return
 */
Status query_est_result_size_serialize(
    Query* query,
    SerializationType serialize_type,
    bool clientside,
    Buffer* serialized_buffer);

/**
 * Deserialize estimated result sizes into the query object
 *
 * @param serialized_buffer Buffer containing serialized query
 * @param serialize_type Serialization type of serialized query
 * @param clientside Whether deserialization should be performed from a client
 *      or server persective
 * @param query Query to deserialize into
 * @return Status
 */
Status query_est_result_size_deserialize(
    Query* query,
    SerializationType serialize_type,
    bool clientside,
    const Buffer& serialized_buffer);

#ifdef TILEDB_SERIALIZATION

enum class SerializationContext { CLIENT, SERVER, BACKUP };

Status global_write_state_to_capnp(
    const Query& query,
    GlobalOrderWriter& global_writer,
    capnp::GlobalWriteState::Builder* state_builder,
    bool client);

Status global_write_state_from_capnp(
    const Query& query,
    const capnp::GlobalWriteState::Reader& state_reader,
    GlobalOrderWriter* global_writer,
    SerializationContext context);

Status unordered_write_state_to_capnp(
    const Query& query,
    UnorderedWriter& unordered_writer,
    capnp::UnorderedWriterState::Builder* state_builder);

Status unordered_write_state_from_capnp(
    const Query& query,
    const capnp::UnorderedWriterState::Reader& state_reader,
    UnorderedWriter* runordered_writer,
    SerializationContext context);

Status condition_from_capnp(
    const capnp::Condition::Reader& condition_reader,
    QueryCondition* const condition);

Status condition_to_capnp(
    const QueryCondition& condition,
    capnp::Condition::Builder* condition_builder);

Status subarray_to_capnp(
    const ArraySchema& schema,
    const Subarray* subarray,
    capnp::Subarray::Builder* builder);

Status subarray_from_capnp(
    const capnp::Subarray::Reader& reader, Subarray* subarray);

void ordered_dim_label_reader_to_capnp(
    const Query& query,
    const OrderedDimLabelReader& reader,
    capnp::QueryReader::Builder* reader_builder);

void ordered_dim_label_reader_from_capnp(
    const capnp::QueryReader::Reader& reader_reader,
    Query* query,
    OrderedDimLabelReader* reader,
    ThreadPool* compute_tp);

Status query_to_capnp(
    Query& query, capnp::Query::Builder* query_builder, const bool client_side);

Status query_from_capnp(
    const capnp::Query::Reader& query_reader,
    const SerializationContext context,
    void* buffer_start,
    CopyState* const copy_state,
    Query* const query,
    ThreadPool* compute_tp,
    const bool allocate_buffers);

#endif

}  // namespace serialization
}  // namespace sm
}  // namespace tiledb

#endif  // TILEDB_SERIALIZATION_QUERY_H
