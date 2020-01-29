#include "./encoder.h"
#include "./custom-types.h"
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cmath>
using std::string;

string to_string(Napi::Env env, Napi::Value val);

Napi::Value decode_erlang(Napi::Env env, char* buff) {
  int index = 0;
  return decode_erlang(env, buff, &index);
}

Napi::Value decode_erlang(Napi::Env env, char* buff, int* index) {
  int type;
  int size;
  ei_get_type(buff, index, &type, &size);
  // see ei.h and http://erlang.org/doc/apps/erts/erl_ext_dist.html
  switch(type) {
    case ERL_SMALL_ATOM_UTF8_EXT:
    case ERL_ATOM_UTF8_EXT:
    case ERL_ATOM_EXT: // includes booleans (atoms 'true' and 'false') and nil
      return decode_atom(env, buff, index);
    case ERL_INTEGER_EXT:
    case ERL_SMALL_INTEGER_EXT:
      return decode_int(env, buff, index);
    case NEW_FLOAT_EXT:
    case ERL_FLOAT_EXT:
      return decode_double(env, buff, index);
    case ERL_SMALL_TUPLE_EXT:
    case ERL_LARGE_TUPLE_EXT:
      return decode_tuple(env, buff, index, size);
    case ERL_LIST_EXT:
      return decode_list(env, buff, index, size);
    case ERL_STRING_EXT:
      return decode_charlist(env, buff, index, size);
    case ERL_BINARY_EXT:
      return decode_binary(env, buff, index, size);
    case ERL_NIL_EXT: // empty list
      (*index)++;
      return Napi::Array::New(env, {});
    case ERL_MAP_EXT:
      return decode_map(env, buff, index, size);
    default:
      return env.Undefined();
  }
}

Napi::Value decode_atom(Napi::Env env, char* buff, int* index) {
  char c[MAXATOMLEN];
  ei_decode_atom(buff, index, c);
  if(strcmp(c, "true") == 0) return Napi::Boolean::New(env, true);
  else if(strcmp(c, "false") == 0) return Napi::Boolean::New(env, false);
  else if(strcmp(c, "nil") == 0) return env.Null();
  else return Atom::Create(Napi::String::New(env, c));
}
Napi::Number decode_int(Napi::Env env, char* buff, int* index) { 
  long n;
  ei_decode_long(buff, index, &n);
  return Napi::Number::New(env, n);
}
Napi::Number decode_double(Napi::Env env, char* buff, int* index) { 
  double n;
  ei_decode_double(buff, index, &n);
  return Napi::Number::New(env, n);
}
Napi::Object decode_map(Napi::Env env,char* buff, int* index, int arity) {
  ei_decode_map_header(buff, index, NULL);
  Napi::Object obj = Napi::Object::New(env);
  for(int i = 0; i<arity; i++) {
    Napi::Value key = decode_erlang(env, buff, index);
    Napi::Value value = decode_erlang(env, buff, index);
    obj.Set(key, value);
  }
  return obj;
}
Napi::Object decode_tuple(Napi::Env env, char* buff, int* index, int arity) {
  ei_decode_tuple_header(buff, index, NULL);
  Napi::Array arr = Napi::Array::New(env, arity);
  for(int i = 0; i<arity; i++) {
    arr[i] = decode_erlang(env, buff, index);
  }
  return Tuple::Create(arr);
}
Napi::Array decode_list(Napi::Env env, char* buff, int* index, int arity) {
  ei_decode_list_header(buff, index, NULL);
  Napi::Array arr = Napi::Array::New(env, arity);
  for(int i = 0; i<arity; i++) {
    arr[i] = decode_erlang(env, buff, index);
  }
  return arr;
}
Napi::Value decode_binary(Napi::Env env, char* buff, int* index, int size) {
  const char * str[size + 1];
  size_t bits;
  unsigned int bitoffs;
  if(ei_decode_bitstring(buff, index, str, &bitoffs, &bits) != 0) {
    Napi::TypeError::New(env, "Could not decode bitstring").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  size_t length = (bitoffs + bits + 7)/8;
  string sstr(*str, length);
  return Napi::String::New(env, sstr);
}
Napi::String decode_charlist(Napi::Env env, char* buff, int* index, int size) {
  char str[size+1];
  ei_decode_string(buff, index, str);
  return Napi::String::New(env, str);
}

bool is_tuple(Napi::Value val) {
  return val.ToObject().InstanceOf(Tuple::constructor.Value());
}
bool is_atom(Napi::Value val) {
  return val.ToObject().InstanceOf(Atom::constructor.Value());
}

void encode_value(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  switch(val.Type()) {
    case napi_string:
      encode_string(env, val, request);
      break;
    case napi_number:
      encode_number(env, val, request);
      break;
    case napi_object: {
      if(val.IsArray()) { encode_array(env, val, request); break; }
      if(is_tuple(val)) { encode_tuple(env, val, request); break; }
      if(is_atom(val)) { encode_atom(env, val, request); break; }
      encode_object(env, val, request);
      break;
    }
    case napi_boolean:
      encode_bool(env, val, request);
      break;
    case napi_null:
    case napi_undefined:
      ei_x_encode_atom(request,"nil");
      return;
    default:
      Napi::Error::New(env, "Cannot encode value: " + to_string(env, val)).ThrowAsJavaScriptException();
      return;
  }
}

void encode_string(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  string arg = val.ToString().Utf8Value();
  std::vector<char> c_arg(arg.begin(), arg.end());
  c_arg.push_back('\0');
  ei_x_encode_bitstring(request, &c_arg[0], 0, strlen(&c_arg[0])*sizeof(char)*8);
}

void encode_number(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  double no = val.ToNumber().DoubleValue();
  if(no == std::trunc(no)) {
    ei_x_encode_long(request, no);
  } else {
    ei_x_encode_double(request, no);
  }
}

void encode_object(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  Napi::Object globject = env.Global().Get("Object").ToObject();
  Napi::Function getSyms = globject.Get("getOwnPropertySymbols").As<Napi::Function>();
  Napi::Function getProps = globject.Get("getOwnPropertyNames").As<Napi::Function>();
  
  Napi::Array symbols = getSyms.Call(globject, {val}).As<Napi::Array>();
  Napi::Array props   = getProps.Call(globject, {val}).As<Napi::Array>();

  ei_x_encode_map_header(request, symbols.Length() + props.Length());
  int symlength = symbols.Length();
  for(int i = 0; i<symlength; i++) {
    Napi::Value symbol = Napi::Value(symbols[i]);
    string arg = symbol.ToObject().Get("description").ToString().Utf8Value();
    std::vector<char> c_arg(arg.begin(), arg.end());
    c_arg.push_back('\0');
    ei_x_encode_atom(request, &c_arg[0]);
    encode_value(env, val.ToObject().Get(symbol.As<Napi::Symbol>()), request);
  }
  int proplength = props.Length();
  for(int i = 0; i<proplength; i++) {
    string arg = Napi::Value(props[i]).ToString().Utf8Value();
    std::vector<char> c_arg(arg.begin(), arg.end());
    c_arg.push_back('\0');
    ei_x_encode_string(request, &c_arg[0]);
    encode_value(env, val.ToObject().Get(Napi::Value(props[i]).ToString()), request);
  }
}

void encode_bool(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  ei_x_encode_boolean(request, (bool)val.ToBoolean());
}

void encode_array(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  Napi::Array arr = val.As<Napi::Array>();
  int length = arr.Length();
  if(length <= 0) {
    ei_x_encode_empty_list(request);
    return;
  }
  ei_x_encode_list_header(request, length);
  for(int i = 0; i<length; i++) {
    encode_value(env, arr[i], request);
  }
  ei_x_encode_empty_list(request);
}
void encode_atom(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  Atom* atom = Napi::ObjectWrap<Atom>::Unwrap(val.As<Napi::Object>());
  string arg = atom->value(env).ToString().Utf8Value();
  std::vector<char> c_arg(arg.begin(), arg.end());
  c_arg.push_back('\0');
  ei_x_encode_atom(request, &c_arg[0]);
}

void encode_tuple(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  Tuple* t = Napi::ObjectWrap<Tuple>::Unwrap(val.As<Napi::Object>());
  Napi::Array arr = t->value(env).As<Napi::Array>();
  int len = arr.Length();
  ei_x_encode_tuple_header(request, len);
  for(int i = 0; i<len; i++) {
    encode_value(env, arr[i], request);
  }
}

string to_string(Napi::Env env, Napi::Value val) {
  Napi::Function fn = val.ToObject().Get("toString").As<Napi::Function>();
  return fn.Call(val, {}).ToString().Utf8Value();
}

