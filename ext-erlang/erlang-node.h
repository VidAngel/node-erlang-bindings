#pragma once
#include <napi.h>
#include <erl_interface.h>
#include <ei.h>
#include <string>

using std::string;

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
