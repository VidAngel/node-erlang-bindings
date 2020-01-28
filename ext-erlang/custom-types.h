#pragma once
#include <napi.h>
#include <string>
using std::string;

class Atom  : public Napi::ObjectWrap<Atom> {
  public:
    Atom(const Napi::CallbackInfo &info);
    static Napi::Object Exports(Napi::Env env, Napi::Object exports);
    Napi::Value value(const Napi::CallbackInfo &info);
    Napi::Value value(Napi::Env env);
    static Napi::FunctionReference constructor;

  private:
    string str;
};

class Tuple : public Napi::ObjectWrap<Tuple>{
  public:
    Tuple(Napi::Value val);
    static Napi::Object Create(Napi::Value arg);
    Tuple(const Napi::CallbackInfo &info);
    static Napi::Object Exports(Napi::Env env, Napi::Object exports);
    Napi::Value value(const Napi::CallbackInfo &info);
    Napi::Value value(Napi::Env env);
    Napi::Value length(const Napi::CallbackInfo &info);
    static Napi::FunctionReference constructor;
    ~Tuple();

  private:
    Napi::Reference<Napi::Array> array;
};
