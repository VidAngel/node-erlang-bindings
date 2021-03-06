#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <thread>
#include <pthread.h>
#include <erl_interface.h>
#include <ei.h>
#include <napi.h>
#include <cstdlib>
#include <string>
#include <arpa/inet.h>
#include <utility>
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

short ErlangNode::creation = 0;

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

  this->pending = true;
  ei_x_buff response;
  ei_x_new(&response);
  int bytes = ei_rpc(&ec, sockfd, &module[0], &method[0], request.buff, request.index, &response);
  ei_x_free(&request);
  this->pending = false;

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
  const Napi::Value result = decode_erlang(env, response.buff);
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
  if(!val->IsArray()) return;
  Napi::Array arr = val->As<Napi::Array>();
  if(arr.Length() != 2) return;
  Napi::Value first = arr[(int)0];
  string desc = first.ToString().Utf8Value();
  if(desc != "badrpc") return;
  // there's got to be a better way...
  Napi::Array subarr = ((Napi::Value)arr[1]).As<Napi::Array>();
  Napi::Array subsubarr = Napi::Value(subarr[1]).As<Napi::Array>();
  string err = ((Napi::Value)subsubarr[(int)0]).ToString().Utf8Value();

  string prefix = "badrpc(" + err + ")";
  if(err == "undef" || err == "function_clause") {
    Napi::TypeError::New(env, prefix + ": Erlang function " + fnName + " not found").ThrowAsJavaScriptException();
  } else if(err == "noproc") {
    Napi::Error::New(env, prefix + ": No remote process found.").ThrowAsJavaScriptException();
  } else {
    Napi::Object json = env.Global().Get("JSON").As<Napi::Object>();
    string suffix = json.Get("stringify").As<Napi::Function>()
                      .Call(json, { val->As<Napi::Object>() })
                      .ToString().Utf8Value();
    Napi::Error::New(env, prefix + ": " + suffix).ThrowAsJavaScriptException();
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

using namespace Napi;
class ErlangListener : public AsyncProgressWorker< std::tuple<int, long, ei_x_buff* > > {
  public:
    ErlangListener(Napi::Function& callback, ErlangNode* node) : AsyncProgressWorker(callback), callback(callback), node(node) {
      this->errnum = -1;
      this->err = false;
    }
    ~ErlangListener(){}
    void Execute(const ExecutionProgress& progress) {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      while(1) {
        if(node->pending) continue;
        if(node->sockfd == -1) break;
        int maxfd = node->sockfd;
        fd_set erlfd;
        FD_ZERO(&erlfd);
        FD_SET(node->sockfd, &erlfd);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10;
        if(select(maxfd+1, &erlfd, NULL, NULL, &tv) <= 0) {
          FD_CLR(node->sockfd, &erlfd);
          FD_ZERO(&erlfd);
          continue;
        }
        ei_x_buff buff;
        erlang_msg msg;
        ei_x_new(&buff);
        int result = ei_xreceive_msg_tmo(node->sockfd, &msg, &buff, 10);
        FD_CLR(node->sockfd, &erlfd);
        FD_ZERO(&erlfd);

        if(result == ERL_ERROR && erl_errno == EAGAIN) {
          // noop
        }
        else if(result == ERL_ERROR && erl_errno != EAGAIN) {
          this->err = true;
          this->errnum = erl_errno;
          break;
        }
        else {
          auto p = std::make_tuple(result, msg.msgtype, &buff);
          progress.Send(&p, 1);
        }
      }
    }
    void OnOK() override {
      if(this->err) {
        Callback().Call(node->Value(), {Napi::String::New(Env(), "error"), Napi::Number::New(Env(), this->errnum)});
      }
    }
    void OnProgress(const std::tuple<int, long, ei_x_buff* > * result, size_t count) override {
      HandleScope scope(Env());
      int type = std::get<0>(*result);

      switch(type) {
        case ERL_TICK:
          Callback().Call(node->Value(), {Napi::String::New(Env(), "tick")});
          break;
        default:
          long msgtype = std::get<1>(*result);
          if(msgtype == ERL_SEND || msgtype == ERL_REG_SEND) {
            Callback().Call(node->Value(), {Napi::String::New(Env(), "message"), decode_erlang(Env(), std::get<2>(*result)->buff)});
          }
          break;
      }
      
    }
  private:
    Napi::Function callback;
    ErlangNode* node;
    bool err;
    int errnum;
};

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

  // dunno if this is necessary or the best way to handle this
  struct in_addr addr;
  addr.s_addr = inet_addr("127.0.0.1");
  string fullAddr = this->node_name + "@127.0.0.1";
  if(ei_connect_xinit(&(this->ec), "127.0.0.1", this->node_name.c_str(), fullAddr.c_str(), &addr, this->cookie.c_str(), creation++) < 0) {
    Napi::Error::New(env, "Could not initialize erlang node").ThrowAsJavaScriptException();
    return;
  }

  Napi::Function cb = info.This().As<Napi::Object>().Get("emit").As<Napi::Function>();
  ErlangListener* el = new ErlangListener(cb, this);
  el->Queue();
  if((this->sockfd = ei_connect(&(this->ec), const_cast<char*>(this->remote_node.c_str()))) < 0) {
    Napi::Error::New(env, "Could not connect to remote node '" + this->remote_node + "'").ThrowAsJavaScriptException();
    return;
  }
}

Napi::Object ErlangNode::Exports(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "ErlangNode", {
    InstanceAccessor("node_name", &ErlangNode::GetName, &ErlangNode::SetName),
    InstanceMethod("disconnect", &ErlangNode::Disconnect),
    InstanceMethod("rpc", &ErlangNode::Call),
  });
  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("ErlangNode", func);
  return exports;
}

