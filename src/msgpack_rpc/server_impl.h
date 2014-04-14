#ifndef NEOVIM_MSGPACK_RPC_SERVER_IMPL_H
#define NEOVIM_MSGPACK_RPC_SERVER_IMPL_H

MsgpackRPCMethodHandler msgpack_rpc_lookup_method_handler(
  const msgpack_rpc_raw *method);

#endif  // NEOVIM_MSGPACK_RPC_SERVER_IMPL_H
