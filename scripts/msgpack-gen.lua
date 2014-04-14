lpeg = require('lpeg')
msgpack = require('cmsgpack')

P, R, S = lpeg.P, lpeg.R, lpeg.S
C, Ct, Cc, Cg = lpeg.C, lpeg.Ct, lpeg.Cc, lpeg.Cg

any = P(1) -- (consume one character)
letter = R('az', 'AZ') + S('_$')
digit = R('09')
alpha = letter + digit
nl = P('\n')
not_nl = any - nl
ws = S(' \t') + nl
fill = ws ^ 0
gap = ws ^ 1

-- C header grammar
c_comment = P('//') * (not_nl ^ 0)
c_preproc = P('#') * (not_nl ^ 0)
c_id = letter * (alpha ^ 0)
c_void = P('void')
c_raw = P('char') * fill * P('*')
c_int = P('uint32_t')
c_array = c_raw * fill * P('*') * Cc('array')
c_param_type = (
  (c_array  * Cc('array') * fill) +
  (c_raw * Cc('raw') * fill) +
  (c_int * Cc('integer') * (ws ^ 1))
  )
c_type = (c_void * Cc('none') * (ws ^ 1)) + c_param_type
c_param = Ct(c_param_type * C(c_id))
c_param_list = c_param * (fill * (P(',') * fill * c_param) ^ 0)
c_params = Ct(c_void + c_param_list)
c_proto = Ct(
  Cg(c_type, 'rtype') * Cg(c_id, 'fname') *
  fill * P('(') * fill * Cg(c_params, 'params') * fill * P(')') *
  fill * P(';')
  )
c_grammar = Ct((c_proto + c_comment + c_preproc + ws) ^ 1)

-- Basic subset of Msgpack IDL
--
-- For the full version:
--   https://github.com/msgpack/msgpack-haskell/blob/master/msgpack-idl/mpidl.peggy
idl_comment = P('#') * (not_nl ^ 0)
idl_id = C(letter * (alpha ^ 0))
idl_int = digit ^ 1
idl_type_param = Ct(Cg(idl_id, 'name'))
idl_type_params = Ct(P('<') * fill *
  idl_type_param * fill * ((P(',') * fill * idl_type_param * fill) ^ 0) *
  P('>'))
idl_ftype_nn = Ct(Cg(idl_id, 'name') * Cg(idl_type_params ^ -1, 'params'))
idl_ftype = idl_ftype_nn -- only non-nullable types for now
idl_rtype = idl_ftype
idl_function_param = Ct(idl_int * fill *
  P(':') * fill *
  Cg(idl_ftype, 'type') * gap *
  Cg(idl_id, 'name') * fill)
idl_function_params =
  Ct((idl_function_param * ((P(',') * fill * idl_function_param) ^ 0)) ^ 0)
idl_function = Ct(Cg(idl_rtype, 'rtype') * gap *
  Cg(idl_id, 'name') * fill *
  P('(') * fill * Cg(idl_function_params, 'params') * P(')') * fill)
idl_service_functions = Ct(idl_function ^ 0)
idl_service_decl = Ct(P('service') * gap * Cg(idl_id, 'name') * fill *
  P('{') * fill * Cg(idl_service_functions, 'functions') * P('}') * fill)

-- Print anything - including nested tables. Useful for debug.
function table_print (tt, indent, done)
  done = done or {}
  indent = indent or 0
  if type(tt) == "table" then
    for key, value in pairs (tt) do
      io.write(string.rep (" ", indent)) -- indent it
      if type (value) == "table" and not done [value] then
        done [value] = true
        io.write(string.format("[%s] => table\n", tostring (key)));
        io.write(string.rep (" ", indent+4)) -- indent it
        io.write("(\n");
        table_print (value, indent + 7, done)
        io.write(string.rep (" ", indent+4)) -- indent it
        io.write(")\n");
      else
        io.write(string.format("[%s] => %s\n", tostring (key), tostring(value)))
      end
    end
  else
    io.write(tt .. "\n")
  end
end

-- Interpolate strings
function interp(s, tab)
  return (s:gsub('($%b{})', function(w) return tab[w:sub(3, -2)] or w end))
end

c_server_preamble = [[#include <stdint.h>
#include <stdlib.h>

#include "msgpack_rpc/common.h"
#include "msgpack_rpc/server_defs.h"
#include "msgpack_rpc/server_impl.h"
#include "api.h"


static const NeovimRPCError wrong_number_of_params_error = {
  .code = kNeovimRPCWrongNumOfParamsError,
  .name = "NeovimRPCWrongNumOfParamsError",
  .message = "Wrong number of parameters."
};

MSGPACK_RPC_DECLARE_EXPECTED_TYPE_ERROR(uint)
MSGPACK_RPC_DECLARE_EXPECTED_TYPE_ERROR(ulong)
MSGPACK_RPC_DECLARE_EXPECTED_TYPE_ERROR(raw)

]]

-- Generate MessagePack-RPC IDL code from C API header file
function gen_msgpack_idl(input)
  local api = c_grammar:match(input:read('*all'))
  local c_to_rpc_type = {
    void = 'void',
    int32_t = 'int',
    uint32_t = 'uint',
    ['char *'] = 'raw',
    ['char **'] = 'list<raw>',
    raw = 'raw',
    none = 'void',
    integer = 'uint',
    array = 'list<raw>'
  }

  io.write('service neovim_service {\n')

  for i = 1, #api do
    local args = {}
    local fn = api[i]

    local ret_type = c_to_rpc_type[fn.rtype]
    io.write('  '..ret_type..' '..fn.fname..'(')
    for j = 1, #fn.params do
      local param = fn.params[j]
      if (j > 1) then
        io.write(', ')
      end
      io.write(j..': '..c_to_rpc_type[param[1]]..' '..param[2])
    end
    io.write(')\n')
  end

  io.write('}')
end

-- Generate C server code from the MessagePack-RPC IDL file
function gen_msgpack_rpc_c_server(input)
  -- Parse the IDL file
  local idl = idl_service_decl:match(input:read('*all'))

  function gen_c_method_handler(fn)
    function idl_type_to_ctype(idl_type)
      local simple_types = {
        void = 'void',
        byte = 'int8_t',
        short = 'int16_t',
        int = 'int32_t',
        long = 'int64_t',
        ubyte = 'uint8_t',
        ushort = 'uint16_t',
        uint = 'uint32_t',
        ulong = 'uint64_t',
        float = 'float',
        double = 'double',
        bool = 'bool',
        raw = 'char*',
        string = 'char*'
      }

      local c_type = simple_types[idl_type.name]
      if c_type then
        return c_type
      end

      if idl_type.name == 'list' then
        local type_param_name = idl_type.params[1].name
        if type_param_name == 'raw' or type_param_name == 'string' then
          return 'char**'
        end
      end

      return nil
    end

    function encode_idl_type_arg(idl_type)
      if idl_type.name == 'list' then
        return 'list_'..encode_idl_type_arg(idl_type.params[1])
      elseif idl_type.name == 'map' then
        return interp('map_${t1}_${t2}',
                      {t1 = encode_idl_type_arg(idl_type.params[1]),
                       t2 = encode_idl_type_arg(idl_type.params[2])})
      end
      return idl_type.name
    end

    local params_list = ''
    local params_unpacking = ''
    local params_freeing = ''
    for i = 1, #fn.params do
      local p = fn.params[i]
      if i > 1 then params_list = params_list..', ' end
      params_list = params_list..'p_'..(i - 1)

      params_unpacking = params_unpacking..interp(
        '  MSGPACK_RPC_CHECK_AND_UNPACK_PARAM(${idl_type}, ${i})\n',
        {idl_type = encode_idl_type_arg(p.type), i = i - 1})

      if p.type.name == 'raw' then
        params_freeing = params_freeing..'  free(p_'..(i - 1)..');\n'
      end
    end

    local pieces = {
      name = fn.name,
      params_len = #fn.params,
      params_unpacking = params_unpacking,
      params_freeing = params_freeing,
      result_ctype = idl_type_to_ctype(fn.rtype),
      params_list = params_list,
      result_idl_type = fn.rtype.name
    }

    local non_void_method_handler = [[
MSGPACK_RPC_METHOD_HANDLER_SIGNATURE(${name})
{
  MSGPACK_RPC_CHECK_PARAMS_LEN(${params_len})
${params_unpacking}  ${result_ctype} api_result = ${name}(${params_list});
${params_freeing}  MSGPACK_RPC_PACK_RES_RESULT(${result_idl_type}, api_result, res)
}

]]
    local void_method_handler = [[
MSGPACK_RPC_METHOD_HANDLER_SIGNATURE(${name})
{
  MSGPACK_RPC_CHECK_PARAMS_LEN(${params_len})
${params_unpacking}  ${name}(${params_list});
${params_freeing}  MSGPACK_RPC_PACK_RES_RESULT_VOID(res)
}

]]
    if fn.rtype.name == 'void' then
      io.write(interp(void_method_handler, pieces))
    else
      io.write(interp(non_void_method_handler, pieces))
    end
  end

  function gen_method_handler_lookup_func(fns)
    io.write([[
MsgpackRPCMethodHandler msgpack_rpc_lookup_method_handler(
  const msgpack_object_raw *method)
{
  char *suffix = method->ptr + 4;
  uint32_t suffix_len = method->size - 4;

#define MSGPACK_RPC_METHOD_COMPARE_AND_RET(method_suffix)      \
  if (strncmp(suffix, "##method_suffix##", suffix_len) == 0) { \
    return _handle_api_##method_suffix;                        \
  }

]])

    for i = 1, #fns do
      io.write(interp([[
  MSGPACK_RPC_METHOD_COMPARE_AND_RET(${suffix})
]], {suffix = fns[i].name:sub(5)}))
    end

    io.write([[
  return NULL;
}]])
  end

  io.write([[// This file is auto-generated by scripts/msgpack-gen.lua
// *** DO NOT EDIT ***

]])
  io.write(c_server_preamble)

  local fns = idl.functions
  for i = 1, #fns do
    gen_c_method_handler(fns[i])
  end

  gen_method_handler_lookup_func(idl.functions)
end


if arg[1] and arg[2] then
  option = arg[1]
  inputf = arg[2]

  local options = {
    ['--idl'] = gen_msgpack_idl,
    ['--c-server'] = gen_msgpack_rpc_c_server,
  }

  local gen_fn = options[option]
  if gen_fn then
    input = io.open(inputf, 'rb')
    gen_fn(input)
    input:close(input)
    return
  end
end

-- Print usage
io.write([[
Usage: lua msgpack-gen.lua --idl C_HEADER_FILE
  or:  lua msgpack-gen.lua OPTION MSGPACK_SPEC_FILE

OPTIONs:
    --c-server         generate C server code
    --c-client         generate C client code
    --cpp-client       generate C++ client code
    --python-client    generate Python client code
]])
