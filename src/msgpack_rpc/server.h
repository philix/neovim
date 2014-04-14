#ifndef NEOVIM_MSGPACK_RPC_SERVER_H
#define NEOVIM_MSGPACK_RPC_SERVER_H

#include <stdbool.h>

#include "msgpack_rpc/common.h"
#include "msgpack_rpc/server_defs.h"

/// Handles an incoming MessagePack-RPC Message.
///
/// Request pipelining isn't possible and async clients won't be any faster.
///
/// @param msg The incoming message pointer (can be a request or a notification)
/// @param res Will be NULL if the message was a notification or will be
///            properly set to the response pointer if the message was a request
/// @return Whether the message was handled successfully
bool msgpack_rpc_handle_msg(const msgpack_object *msg,
                            msgpack_packer **res);


#endif  // NEOVIM_MSGPACK_RPC_SERVER_H
