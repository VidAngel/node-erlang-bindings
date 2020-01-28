#pragma once
#include <napi.h>
#include <string>
using std::string;

class Atom  : public Napi::ObjectWrap<Atom> {
  public:
    Atom(const Napi::CallbackInfo &info);
    static Napi::Object Exports(Napi::Env env, Napi::Object exports);
    Napi::Value value(const Napi::CallbackInfo &info);
    static Napi::FunctionReference constructor;

  private:
    string str;
};

class Tuple : public Napi::ObjectWrap<Tuple>{
  public:
    Tuple(const Napi::CallbackInfo &info);
    static Napi::Object Exports(Napi::Env env, Napi::Object exports);
    Napi::Value value(const Napi::CallbackInfo &info);
    static Napi::FunctionReference constructor;
    ~Tuple();

  private:
    Napi::Reference<Napi::Array> array;
};
