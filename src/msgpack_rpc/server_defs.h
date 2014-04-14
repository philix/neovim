#ifndef NEOVIM_MSGPACK_RPC_SERVER_UTILS_H
#define NEOVIM_MSGPACK_RPC_SERVER_UTILS_H

#include <stdbool.h>

#include <msgpack.h>

#include "common.h"

/// Method handler function pointer type
typedef bool (*MsgpackRPCMethodHandler)(uint32_t message_id,
                                        const msgpack_object_array *params,
                                        const msgpack_packer *res);

#define MSGPACK_RPC_METHOD_HANDLER_SIGNATURE(name)                             \
static bool _handle_##name(uint32_t message_id,                                \
                           const msgpack_object_array *params,                 \
                           const msgpack_packer *res)

// Check the length of parameter array.
#define MSGPACK_RPC_CHECK_PARAMS_LEN(len)                                      \
  if (params->size != (len)) {                                                 \
    if (req) {                                                                 \
      msgpack_rpc_build_error_res(message_id,                                  \
                                  &wrong_number_of_params_error, res);         \
    }                                                                          \
    return false;                                                              \
  }

// Use to declare NeovimRPCError constants.
// idl_type is one of the MessagePack-RPC IDL types and will be used to build
// the client error messages.
#define MSGPACK_RPC_DECLARE_EXPECTED_TYPE_ERROR(idl_type)                      \
  static const NeovimRPCError expected_##idl_type##_param_error = {            \
    .code = kNeovimRPCWrongParamTypeError,                                     \
    .name = "kNeovimRPCWrongParamTypeError",                                   \
    .message = "Wrong parameter type: expected" ##idl_type## "."               \
  };

#define MSGPACK_RPC_EXPECTED_TYPE_ERROR(idl_type) \
  expected_##idl_type##_param_error

// Check the expected type of the idx-th param type.
//
// All the NeovimRPCError constants should be previously declared using
// MSGPACK_RPC_DECLARE_EXPECTED_TYPE_ERROR.
#define MSGPACK_RPC_CHECK_PARAM_TYPE(idx, idl_type, msgpack_type_id)           \
  if (params->ptr[(idx)].type != (msgpack_type_id)) {                          \
    if (req) {                                                                 \
      msgpack_rpc_build_error_res(message_id,                                  \
                                  &MSGPACK_RPC_EXPECTED_TYPE_ERROR(idl_type),  \
                                  res);                                        \
    }                                                                          \
    return false;                                                              \
  }

// MSGPACK_RPC_UNPACK_PARAM_* macros used to declare the properly typed
// p_##idx local variable.

#define MSGPACK_RPC_UNPACK_PARAM_uint(idx)                                     \
  MSGPACK_RPC_CHECK_PARAM_TYPE(idx, uint, MSGPACK_OBJECT_POSITIVE_INTEGER)     \
  uint32_t p_##idx = (uint32_t) params[(idx)].via.i64;

#define MSGPACK_RPC_UNPACK_PARAM_raw(idx)                                      \
  MSGPACK_RPC_CHECK_PARAM_TYPE(raw, MSGPACK_OBJECT_RAW)                        \
  char *p_#idx = xstrndup(params[(idx)].via.raw.ptr,                           \
                              params[(idx)].via.raw.size);

// Check the type using MSGPACK_RPC_CHECK_PARAM_TYPE and all the
// MSGPACK_RPC_UNPACK_PARAM_* macros.
#define MSGPACK_RPC_CHECK_AND_UNPACK_PARAM(idx, idl_type, msgpack_type_id)     \
  MSGPACK_RPC_CHECK_PARAM_TYPE(idx, idl_type, msgpack_type_id)                 \
  MSGPACK_RPC_UNPACK_PARAM_##idl_type(idx)

// Pack the first three fields of the Response Message array.
#define MSGPACK_RPC_INIT_SUCCESS_RES(res)                                     \
  msgpack_pack_array(res, 4);                                                 \
  msgpack_pack_uint32(res, kResponseRPCMessageType);                          \
  msgpack_pack_uint32(res, message_id);                                       \
  msgpack_pack_nil(res);

// Pack the result field of the Response Message array. It uses the
// MSGPACK_RPC_PACK_* macros.
#define MSGPACK_RPC_SUCCESS_RES_RESULT(idl_type, api_result, res)            \
  if (res) {                                                                 \
    MSGPACK_RPC_INIT_SUCCESS_RES(res)                                        \
    MSGPACK_RPC_PACK_##idl_type(api_result, res)                             \
  }                                                                          \
  return true;

#define MSGPACK_RPC_SUCCESS_RES_RESULT_VOID(res)                             \
  if (res) {                                                                 \
    MSGPACK_RPC_INIT_SUCCESS_RES(res)                                        \
    msgpack_pack_nil(res);                                                   \
  }                                                                          \
  return true;

#endif  // NEOVIM_MSGPACK_RPC_SERVER_UTILS_H
