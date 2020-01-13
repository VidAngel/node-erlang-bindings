#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <erl_interface.h>
#include <ei.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <napi.h>
#include <iostream>
#include <cmath>
#include <thread>
#include <pthread.h>
#include <chrono>
#include "unistd.h"
using std::string;
using std::cout;
using std::endl;
class ErlangNode : public Napi::ObjectWrap<ErlangNode> {
  public:
    ErlangNode(const Napi::CallbackInfo &info);
    ~ErlangNode() { this->_disconnect(); }
    static Napi::Object Exports(Napi::Env env, Napi::Object exports);

  private:
    int sockfd = -1;
    static Napi::FunctionReference constructor;
    Napi::Value GetName(const Napi::CallbackInfo &info) { return Napi::String::New(info.Env(), this->node_name);}
    void SetName(const Napi::CallbackInfo &info, const Napi::Value &value) { this->node_name = value.As<Napi::String>();}
    string node_name;
    string cookie;
    string remote_node;
    ei_cnode ec;
    Napi::Value Connect(const Napi::CallbackInfo &info);
    void Disconnect(const Napi::CallbackInfo &info) { this->_disconnect(); }
    void _disconnect();
    Napi::Value Call(const Napi::CallbackInfo &info);
    void Listen();
};

string to_string(Napi::Env env, Napi::Value val);
void add_value(Napi::Env env, Napi::Value val, ei_x_buff* request);
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term, int* index);
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term);

void ErlangNode::_disconnect() {
 if(this->sockfd != -1) ei_close_connection(this->sockfd);
 this->sockfd = -1;
}

void add_string(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  string arg = val.ToString().Utf8Value();
  std::vector<char> c_arg(arg.begin(), arg.end());
  c_arg.push_back('\0');
  ei_x_encode_bitstring(request, &c_arg[0], 0, strlen(&c_arg[0])*sizeof(char)*8);
}

void add_atom(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  string arg = val.ToObject().Get("description").ToString().Utf8Value();
  std::vector<char> c_arg(arg.begin(), arg.end());
  c_arg.push_back('\0');
  ei_x_encode_atom(request, &c_arg[0]);
}

void add_tuple(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  Napi::Array arr = val.As<Napi::Array>();
  int len = arr.Length();
  ei_x_encode_tuple_header(request, len);
  for(int i = 0; i<len; i++) {
    add_value(env, arr[i], request);
  }
}

string to_string(Napi::Env env, Napi::Value val) {
  Napi::Function fn = val.ToObject().Get("toString").As<Napi::Function>();
  return fn.Call(val, {}).ToString().Utf8Value();
}

void add_number(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  double no = val.ToNumber().DoubleValue();
  if(no == std::trunc(no)) {
    ei_x_encode_long(request, no);
  } else {
    ei_x_encode_double(request, no);
  }
}

void add_object(Napi::Env env, Napi::Value val, ei_x_buff* request) {
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
    add_value(env, val.ToObject().Get(symbol.As<Napi::Symbol>()), request);
  }
  int proplength = props.Length();
  for(int i = 0; i<proplength; i++) {
    string arg = Napi::Value(props[i]).ToString().Utf8Value();
    std::vector<char> c_arg(arg.begin(), arg.end());
    c_arg.push_back('\0');
    ei_x_encode_string(request, &c_arg[0]);
    add_value(env, val.ToObject().Get(Napi::Value(props[i]).ToString()), request);
  }
}

void add_bool(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  ei_x_encode_boolean(request, (bool)val.ToBoolean());
}

void add_array(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  Napi::Array arr = val.As<Napi::Array>();
  int length = arr.Length();
  if(length <= 0) {
    ei_x_encode_empty_list(request);
    return;
  }
  ei_x_encode_list_header(request, length);
  for(int i = 0; i<length; i++) {
    add_value(env, arr[i], request);
  }
  ei_x_encode_empty_list(request);
}

bool is_tuple(Napi::Env env, Napi::Value val) {
  Napi::Value constructor = val.ToObject().Get("constructor");
  if(constructor.IsUndefined()) return false;
  if(constructor.ToObject().Get("name").ToString() == Napi::String::New(env, "Tuple")) return true;
  return false;
}

void check_status(Napi::Env env, const Napi::Value *val, string fnName) {
  if(!val->IsArray()) return;
  Napi::Array arr = val->As<Napi::Array>();
  if(arr.Length() != 2) return;
  Napi::Value first = arr[(int)0];
  if(first.Type() != napi_symbol) return;
  string desc = first.ToObject().Get("description").ToString().Utf8Value();
  if(desc != "badrpc") return;
  // there's got to be a better way...
  Napi::Array subarr = ((Napi::Value)arr[1]).As<Napi::Array>();
  Napi::Array subsubarr = Napi::Value(subarr[1]).As<Napi::Array>();
  string err = ((Napi::Value)subsubarr[(int)0]).ToObject().Get("description").ToString().Utf8Value();

  string prefix = "badrpc(" + err + ")";
  if(err == "undef") {
    Napi::TypeError::New(env, prefix + ": Erlang function " + fnName + " not found").ThrowAsJavaScriptException();
  } else if(err == "noproc") {
    Napi::Error::New(env, prefix + ": No remote process found.").ThrowAsJavaScriptException();
  } else {
    Napi::Error::New(env, prefix).ThrowAsJavaScriptException();
  }
}

void add_value(Napi::Env env, Napi::Value val, ei_x_buff* request) {
  switch(val.Type()) {
    case napi_string:
      add_string(env, val, request);
      break;
    case napi_number:
      add_number(env, val, request);
      break;
    case napi_symbol:
      add_atom(env, val, request);
      break;
    case napi_object: {
      if(is_tuple(env, val)) { add_tuple(env, val, request); break; }
      if(val.IsArray()) { add_array(env, val, request); break; }
      add_object(env, val, request);
      break;
    }
    case napi_boolean:
      add_bool(env, val, request);
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

ei_x_buff encode_args(const Napi::CallbackInfo &info, int index) {
  int argc = info.Length();
  Napi::Env env = info.Env();
  ei_x_buff request;
  ei_x_new(&request);
  ei_x_encode_list_header(&request, argc-2);

  for(int i = index; i < argc; i++) {
    add_value(env, info[i], &request);
  }
  ei_x_encode_empty_list(&request);
  return request;
}
Napi::Value decode_atom(Napi::Env env, ei_x_buff* term, int* index) {
  char c[MAXATOMLEN];
  ei_decode_atom(term->buff, index, c);
  if(strcmp(c, "true") == 0) return Napi::Boolean::New(env, true);
  else if(strcmp(c, "false") == 0) return Napi::Boolean::New(env, false);
  else if(strcmp(c, "nil") == 0) return env.Null();
  else return Napi::Symbol::New(env, c);
}
Napi::Number decode_int(Napi::Env env,ei_x_buff* term, int* index) { 
  long n;
  ei_decode_long(term->buff, index, &n);
  return Napi::Number::New(env, n);
}
Napi::Number decode_double(Napi::Env env,ei_x_buff* term, int* index) { 
  double n;
  ei_decode_double(term->buff, index, &n);
  return Napi::Number::New(env, n);
}
Napi::Object decode_map(Napi::Env env,ei_x_buff* term, int* index, int arity) {
  ei_decode_map_header(term->buff, index, NULL);
  Napi::Object obj = Napi::Object::New(env);
  for(int i = 0; i<arity; i++) {
    Napi::Value key = decode_erlang(env, term, index);
    Napi::Value value = decode_erlang(env, term, index);
    obj.Set(key, value);
  }
  return obj;
}
Napi::Array decode_tuple(Napi::Env env, ei_x_buff* term, int* index, int arity) {
  ei_decode_tuple_header(term->buff, index, NULL);
  Napi::Array arr = Napi::Array::New(env, arity);
  for(int i = 0; i<arity; i++) {
    arr[i] = decode_erlang(env, term, index);
  }
  return arr;
}
Napi::Array decode_list(Napi::Env env, ei_x_buff* term, int* index, int arity) {
  ei_decode_list_header(term->buff, index, NULL);
  Napi::Array arr = Napi::Array::New(env, arity);
  for(int i = 0; i<arity; i++) {
    arr[i] = decode_erlang(env, term, index);
  }
  return arr;
}
Napi::Value decode_binary(Napi::Env env, ei_x_buff* term, int* index, int size) {
  const char * str[size + 1];
  size_t bits;
  unsigned int bitoffs;
  if(ei_decode_bitstring(term->buff, index, str, &bitoffs, &bits) != 0) {
    Napi::TypeError::New(env, "Could not decode bitstring").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  size_t length = (bitoffs + bits + 7)/8;
  string sstr(*str, length);
  return Napi::String::New(env, sstr);
}
Napi::String decode_charlist(Napi::Env env, ei_x_buff* term, int* index, int size) {
  char str[size+1];
  ei_decode_string(term->buff, index, str);
  return Napi::String::New(env, str);
}

Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term, int* index) {
  int type;
  int size;
  ei_get_type(term->buff, index, &type, &size);
  // see ei.h and http://erlang.org/doc/apps/erts/erl_ext_dist.html
  switch(type) {
    case ERL_SMALL_ATOM_UTF8_EXT:
    case ERL_ATOM_UTF8_EXT:
    case ERL_ATOM_EXT: // includes booleans (atoms 'true' and 'false') and nil
      return decode_atom(env, term, index);
    case ERL_INTEGER_EXT:
    case ERL_SMALL_INTEGER_EXT:
      return decode_int(env, term, index);
    case NEW_FLOAT_EXT:
    case ERL_FLOAT_EXT:
      return decode_double(env, term, index);
    case ERL_SMALL_TUPLE_EXT:
    case ERL_LARGE_TUPLE_EXT:
      return decode_tuple(env, term, index, size);
    case ERL_LIST_EXT:
      return decode_list(env, term, index, size);
    case ERL_STRING_EXT:
      return decode_charlist(env, term, index, size);
    case ERL_BINARY_EXT:
      return decode_binary(env, term, index, size);
    case ERL_NIL_EXT: // empty list
      (*index)++;
      return Napi::Array::New(env, {});
    case ERL_MAP_EXT:
      return decode_map(env, term, index, size);
    default:
      return env.Undefined();
  }
}
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term) {
  int index = 0;
  return decode_erlang(env, term, &index);
}
Napi::Value ErlangNode::Call(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if(info.Length() < 2) {
    Napi::TypeError::New(env, "Module and method expected as arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  string module = info[0].ToString().Utf8Value();
  string method = info[1].ToString().Utf8Value();
  std::vector<char> c_module(module.begin(), module.end());
  c_module.push_back('\0');
  std::vector<char> c_method(method.begin(), method.end());
  c_method.push_back('\0');

  ei_x_buff request = encode_args(info, 2);

  ei_x_buff response;
  ei_x_new(&response);
  int bytes = ei_rpc(&ec, sockfd, &module[0], &method[0], request.buff, request.index, &response);
  ei_x_free(&request);

  if(bytes == -1) {
    string message;
    switch(erl_errno) {
      case EIO:
        message = "IO Error";
        break;
      case ETIMEDOUT:
        message = "Timeout";
        break;
      case EAGAIN:
        message = "Temporary error, please try again.";
        break;
      default:
        message = "Unknown error";
    }
    Napi::TypeError::New(env, "Error calling remote method " + (string)module + "." + (string)method + ": " + message).ThrowAsJavaScriptException();
    ei_x_free(&response);
    return env.Undefined();
  }
  const Napi::Value result = decode_erlang(env, &response);
  ei_x_free(&response);
  check_status(env, &result, (string)module + "." + (string)method);
  return result;
}

Napi::Value ErlangNode::Connect(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  // dunno if this is necessary or the best way to handle this
  short creation = rand() % 256;
  if(ei_connect_init(&(this->ec), this->node_name.c_str(), this->cookie.c_str(), creation) < 0) {
    Napi::Error::New(env, "Could not initialize erlang node").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  if((this->sockfd = ei_connect(&(this->ec), const_cast<char*>(this->remote_node.c_str()))) < 0) {
    Napi::Error::New(env, "Could not connect to remote node '" + this->remote_node + "'").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  std::thread listener(&ErlangNode::Listen, std::ref(*this));
  listener.detach();
  return Napi::Boolean::New(env, true);
}

void ErlangNode::Listen() {
  while(1) {
    std::this_thread::sleep_for (std::chrono::seconds(1));
    int maxfd = this->sockfd;
    fd_set erlfd;
    FD_ZERO(&erlfd);
    FD_SET(this->sockfd, &erlfd);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10;
    if(select(maxfd+1, &erlfd, NULL, NULL, &tv) <= 0) {
      FD_CLR(this->sockfd, &erlfd);
      FD_ZERO(&erlfd);
      continue;
    }
    erlang_msg msg;
    ei_x_buff buff;
    ei_x_new(&buff);
    int result = ei_xreceive_msg_tmo(sockfd, &msg, &buff, 10);
    FD_CLR(this->sockfd, &erlfd);
    FD_ZERO(&erlfd);
    switch(result) {
      case ERL_TICK:
        // TODO
        break;
      case EAGAIN:
      case EMSGSIZE:
      case EIO:
      case ERL_ERROR:
        // TODO
        break;
      default:
        // TODO
        break;
    }
  }
}

Napi::FunctionReference ErlangNode::constructor;
ErlangNode::ErlangNode(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ErlangNode>(info) {
  Napi::Env env = info.Env();
  if(info.Length() != 3) {
    Napi::TypeError::New(env, "Wrong number of arguments. Expected: node name, cookie, remote node").ThrowAsJavaScriptException();
    return;
  }
  this->node_name = info[0].As<Napi::String>();
  this->cookie = info[1].As<Napi::String>();
  this->remote_node = info[2].As<Napi::String>();
}

Napi::Object ErlangNode::Exports(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "ErlangNode", {
    InstanceAccessor("node_name", &ErlangNode::GetName, &ErlangNode::SetName),
    InstanceMethod("connect", &ErlangNode::Connect),
    InstanceMethod("disconnect", &ErlangNode::Disconnect),
    InstanceMethod("rpc", &ErlangNode::Call),
  });
  exports.Set("ErlangNode", func);
  return exports;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  ei_init();
  ErlangNode::Exports(env, exports);
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
