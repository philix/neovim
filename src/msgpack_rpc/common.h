/// @file common.h Common definitions for clients and servers.

#ifndef NEOVIM_MSGPACK_RPC_COMMON_H
#define NEOVIM_MSGPACK_RPC_COMMON_H

// # MessagePack-RPC Protocol specification
//
// The protocol consists of "Request" message and the corresponding "Response"
// message. The server must send "Response" message in reply with the "Request"
// message.
//
//
// ## Request Message
//
// The request message is a four elements array shown below, packed by
// MessagePack format.
//
//     [type, msgid, method, params]
//
// ### type
//
// Must be zero (integer). Zero means that this message is the "Request"
// message.
//
// ### msgid
//
// The 32-bit unsigned integer number. This number is used as a sequence number.
// The server replies with a requested msgid.
//
// ### method
//
// The string, which represents the method name.
//
// ### params
//
// The array of the function arguments. The elements of this array is arbitrary
// object.
//
//
// ## Response Message
//
// The response message is a four elements array shown below, packed by
// MessagePack format.
//
//     [type, msgid, error, result]
//
// ### type
//
// Must be one (integer). One means that this message is the "Response" message.
//
// ### msgid
//
// The 32-bit unsigned integer number. This corresponds to the request message.
//
// ### error
//
// If the method is executed correctly, this field is Nil. If the error occurred
// at the server-side, then this field is an arbitrary object which represents
// the error.
//
// ### result
//
// An arbitrary object, which represents the returned result of the function. If
// error occurred, this field should be nil.
//
//
// ## Notification Message
//
// The notification message is a three elements array shown below, packed by
// MessagePack format.
//
//    [type, method, params]
//
// ### type
//
// Must be two (integer). Two means that this message is the "Notification"
// message.
//
// ### method
//
// The string, which represents the method name.
//
// ### params
//
// The array of the function arguments. The elements of this array is arbitrary
// object.
//
//
// ## The Order of the Response
//
// The server implementations don't need to send the reply, in the order of the
// received requests. If they receive the multiple messages, they can reply in
// random order.
//
// This is required for the pipelining. At the server side, some functions are
// fast, and some are not. If the server must reply with in order, the slow
// functions delay the other replies even if it's execution is already
// completed.

// MessagePack-IDL Types
//
// We try to use the MessagePack-RPC IDL type nomenclature whenever possible in
// server and client code.
//
// Basic Types
//
//  - void
//  - byte   : signed 8-bit integer
//  - short  : signed 16-bit integer
//  - int    : signed 32-bit integer
//  - long   : signed 64-bit integer
//  - ubyte  : unsigned 8-bit integer
//  - ushort : unsigned 16-bit integer
//  - uint   : unsigned 32-bit integer
//  - ulong  : unsigned 64-bit integer
//  - float  : single precision float
//  - double : double precision float
//  - bool   : boolean
//  - raw    : raw bytes
//  - string : string
//
// Container Types
//
//  - list<string>                 : list
//  - map<string, int>             : map
//  - map<string, list<string>>    : nesting is ok

#include <stdint.h>
#include <string.h>

enum neovim_rpc_message_types {
  kRequestRPCMessageType = 0,
  kResponseRPCMessageType = 1,
  kNotificationRPCMessageType = 2
};

typedef enum {
  kNeovimRPCError = 0, // general error
  kNeovimRPCMalformedMessageError = 1,
  kNeovimRPCNoMethodError = 2,
  kNeovimRPCWrongNumOfParamsError = 3,
  kNeovimRPCWrongParamTypeError = 4
} NeovimRPCErrorCode;

#ifndef NO_NEOVIM_RPC_ERROR_NAMES
const char *neovim_rpc_error_name[] = {
  [kNeovimRPCError] = "NeovimeRPCError",
  [kNeovimRPCMalformedMessageError] = "NeovimRPCMalformedMessageError",
  [kNeovimRPCNoMethodError] = "NeovimRPCNoMethodError",
  [kNeovimRPCWrongNumOfParamsError] = "NeovimRPCWrongNumOfParamsError",
  [kNeovimRPCWrongParamTypeError] = "NeovimRPCWrongParamTypeError"
};
#endif

// The MessagePack-RPC spec allows the server to return an arbitrary object
// which represents the error.
//
// Neovim RPC server always return a msgpack-encoded 3-element array of the
// following format to represent an error:
//
//     [code, name, message]
//
// code: is one of the unsigned integers from NeovimRPCErrorCode
// enum.
//
// name: the name of the error. It can be used to name exception classes in
// client code (e.g. NeovimRPCMalformedMessageError).
//
// message: a human readable error message.
typedef struct {
  NeovimRPCErrorCode code;
  char *name;
  char *message;
} NeovimRPCError;

typedef struct {
  uint32_t last_message_id;
  NeovimeRPCError *last_error;
} NeovimRPCSession;

// MessagePack data packing macros. The name uses MessagePack-RPC IDL type names
// as suffix. For types like list<raw> the suffix is list_raw.
//
// @param v the value(or pointer) being packed
// @param p the msgpack_packer
#define MSGPACK_RPC_PACK_uint(v, p) msgpack_pack_uint32((p), (v));
#define MSGPACK_RPC_PACK_raw(v, p) msgpack_pack_raw((p), (v), strlen(v));
#define MSGPACK_RPC_PACK_list_raw(v, p)                                        \
  {                                                                            \
    uint32_t len = 0;                                                          \
    for (char **s = v; s; s++) len++;                                          \
    msgpack_pack_array((p), len);                                              \
    for (char **s = v; s; s++) msgpack_pack_raw((p), strlen(*s));              \
  }
#define MSGPACK_RPC_PACK_list_string(v, p) MSGPACK_RPC_PACK_list_raw(v, p);

#endif  // NEOVIM_MSGPACK_RPC_COMMON_H
