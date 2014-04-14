#include <stdbool.h>
#include <stdint.h>

#include <msgpack.h>

#include "msgpack_rpc/common.h"
#include "msgpack_rpc/server.h"
#include "msgpack_rpc/server_defs.h"
#include "msgpack_rpc/server_impl.h"

static const NeovimRPCError msgpack_rpc_malformed_msg_error = {
  .code = kNeovimRPCMalformedMessageError,
  .name = "NeovimRPCMalformedMessageError",
  .message =
      "MessagePack-RPC message does not follow the standard \
format ([type, msgid, method, params]) or is invalid."
};

bool msgpack_rpc_handle_msg(const msgpack_object *msg, msgpack_packer **res)
{
  // Validate the basic structure of the msgpack-rpc payload
  if (msg->type != MSGPACK_OBJECT_ARRAY ||
      msg->via.array.size != 4 ||
      msg->via.array.ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER ||
      (msg->via.array.ptr[0].u64 != kRequestRPCMessageType &&
       msg->via.array.ptr[0].u64 != kNotificationRPCMessageType) ||
      msg->via.array.ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER ||
      msg->via.array.ptr[2].type != MSGPACK_OBJECT_POSITIVE_RAW ||
      msg->via.array.ptr[3].type != MSGPACK_OBJECT_ARRAY) {
    bool is_notification = false;
    if (msg->type == MSGPACK_OBJECT_ARRAY && msg->via.array.size >= 1) {
      is_notification =
        msg->via.array.ptr[0].u64 == kNotificationRPCMessageType;
    }
    if (is_notification) {
      *res = NULL;
    } else {
      // try to recover the message id to build the error response
      uint32_t message_id = 0;
      if (msg->type == MSGPACK_OBJECT_ARRAY &&
          msg->via.array.size >= 2 &&
          (msg->via.array.ptr[1].type == MSGPACK_OBJECT_POSITIVE_INTEGER ||
           msg->via.array.ptr[1].type == MSGPACK_OBJECT_NEGATIVE_INTEGER)) {

        if (msg->via.array.ptr[1].type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
          message_id = (uint32_t) msg->via.array.ptr[1].via.u64;
        } else {
          message_id = (uint32_t) msg->via.array.ptr[1].via.i64;
        }
      }

      msgpack_rpc_build_error_res(message_id, &msgpack_rpc_malformed_msg_error,
                                  *res);
    }
    return false;
  }

  const uint32_t type = (uint32_t) msg->via.array.ptr[0].u64;
  const uint32_t message_id = (uint32_t) msg->via.array.ptr[1].u64;
  const msgpack_object_raw *method = &msg.via.array.ptr[2].raw;
  const msgpack_object_array *params = &msg.via.array.ptr[3].array;

  handle_message_fn = msgpack_rpc_lookup_method_handler(method);
  if (type == kNotificationRPCMessageType) {
    *res = NULL;
  }
  return handle_message_fn(params, *res);
}
