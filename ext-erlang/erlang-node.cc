#include <thread>
#include <pthread.h>
#include <erl_interface.h>
#include <ei.h>
#include <napi.h>
#include <cstdlib>
#include <string>
#include "./custom-types.h"
#include "./erlang-node.h"
#include "./encoder.h"

using std::string;

void check_status(Napi::Env env, const Napi::Value *val, string fnName);
ei_x_buff encode_args(const Napi::CallbackInfo &info, int index);

void ErlangNode::_disconnect() {
 if(this->sockfd != -1) ei_close_connection(this->sockfd);
 this->sockfd = -1;
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
        fprintf(stderr, "OTHER MESSAGE\n");
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

void check_status(Napi::Env env, const Napi::Value *val, string fnName) {
  // a bad status will look like this:
  /**
   * Tuple {
   *   Atom { 'badrpc' },
   *   Tuple {
   *     Atom { 'EXIT' },
   *     Tuple {
   *       Atom { 'noproc' },
   *       Tuple {
   *         Atom { 'Elixir.GenServer' },
   *         Atom { 'call' },
   *         [ Atom { 'SomeFakeProcessName' }, Atom { 'fakeaction' }, 5000 ]
   *       }
   *     }
   *   }
   * }
   */
  
  if(val->IsNull()) return;
  if(!is_tuple(*val)) return;
  Tuple* t = unwrap<Tuple>(*val);
  Napi::Array arr = t->value(env).As<Napi::Array>();
  if(arr.Length() != 2) return;
  Napi::Value first = arr[(int)0];
  if(!is_atom(first)) return;
  string desc = first.ToString().Utf8Value();
  if(desc != "badrpc") return;

  Tuple* sub = unwrap<Tuple>(arr[1]);
  Tuple* subsub = unwrap<Tuple>(sub->value(env).As<Napi::Array>()[1]);
  Napi::Array subsubarr = subsub->value(env).As<Napi::Array>();
  string err = ((Napi::Value)subsubarr[(int)0]).ToString().Utf8Value();

  string prefix = "badrpc(" + err + ")";
  if(err == "undef") {
    Napi::TypeError::New(env, prefix + ": Erlang function " + fnName + " not found").ThrowAsJavaScriptException();
  } else if(err == "noproc") {
    Napi::Error::New(env, prefix + ": No remote process found.").ThrowAsJavaScriptException();
  } else {
    Napi::Error::New(env, prefix).ThrowAsJavaScriptException();
  }
}

ei_x_buff encode_args(const Napi::CallbackInfo &info, int index) {
  int argc = info.Length();
  Napi::Env env = info.Env();
  ei_x_buff request;
  ei_x_new(&request);
  ei_x_encode_list_header(&request, argc-2);

  for(int i = index; i < argc; i++) {
    encode_value(env, info[i], &request);
  }
  ei_x_encode_empty_list(&request);
  return request;
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
        if(this->recv > 0) {
          // TODO
          //this->onReceive.MakeCallback(Napi::Object::New(this->onReceiveCtx->Env()), {Napi::String::New(this->onReceiveCtx->Env(), "hello world")}, *(this->onReceiveCtx));
        }
        fprintf(stderr, "TICK\n");
        break;
      case ERL_ERROR:
        fprintf(stderr, "Error reading Erlang message: %d\n",  erl_errno);
        break;
      default:
        // TODO
        break;
    }
  }
}

Napi::FunctionReference ErlangNode::constructor;
ErlangNode::ErlangNode(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ErlangNode>(info) {
  ei_init();
  Napi::Env env = info.Env();
  if(info.Length() != 3) {
    Napi::TypeError::New(env, "Wrong number of arguments. Expected: node name, cookie, remote node").ThrowAsJavaScriptException();
    return;
  }
  this->node_name = info[0].As<Napi::String>();
  this->cookie = info[1].As<Napi::String>();
  this->remote_node = info[2].As<Napi::String>();
}
Napi::Value ErlangNode::Receive(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if(info.Length() < 1) {
    Napi::TypeError::New(env, "Missing callback function").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  if(info[0].Type() != napi_function) {
    Napi::TypeError::New(env, "Argument must be a function").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  if(this->recv > 0) {
    this->onReceive.Unref();
  }
  this->recv = 1;
  Napi::AsyncContext ctx = Napi::AsyncContext(env, "erl_async_context");
  this->onReceiveCtx = &ctx;
  this->onReceive = Napi::Persistent(info[0].As<Napi::Function>());
  //this->onReceive.MakeCallback(Napi::Object::New(this->onReceiveCtx->Env()), {Napi::String::New(this->onReceiveCtx->Env(), "hello world")}, *(this->onReceiveCtx));
  return env.Undefined();
}

Napi::Object ErlangNode::Exports(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "ErlangNode", {
    InstanceAccessor("node_name", &ErlangNode::GetName, &ErlangNode::SetName),
    InstanceMethod("connect", &ErlangNode::Connect),
    InstanceMethod("disconnect", &ErlangNode::Disconnect),
    InstanceMethod("rpc", &ErlangNode::Call),
    InstanceMethod("receive", &ErlangNode::Receive),
  });
  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("ErlangNode", func);
  return exports;
}

