#pragma once
#include <napi.h>
#include <string>
using std::string;

class Atom  : public Napi::ObjectWrap<Atom> {
  public:
    Atom(const Napi::CallbackInfo &info);
    static Napi::Object Exports(Napi::Env env, Napi::Object exports);
    Napi::Value value(const Napi::CallbackInfo &info) { return Napi::String::New(info.Env(), this->str); }

  private:
    static Napi::FunctionReference constructor;
    string str;
};
class Tuple : public Napi::ObjectWrap<Tuple>{
  public:
    Tuple(const Napi::CallbackInfo &info);
    static Napi::Object Exports(Napi::Env env, Napi::Object exports);
    Napi::Value value(const Napi::CallbackInfo &info) { return this->array.Value(); }
    ~Tuple() { this->array.Unref(); }

  private:
    static Napi::FunctionReference constructor;
    Napi::Reference<Napi::Array> array;
};
Atom::Atom(const Napi::CallbackInfo &info)  : Napi::ObjectWrap<Atom>(info) {
  if(info.Length() <= 0) {
    Napi::Env env = info.Env();
    Napi::TypeError::New(env, "Atom constructor requires value").ThrowAsJavaScriptException();
  }
  this->str = info[0].ToString().Utf8Value();
}
Napi::Object Atom::Exports(Napi::Env env, Napi::Object exports) {
  Napi::Function atom = DefineClass(env, "Atom", {
    InstanceAccessor("value", &Atom::value, NULL),
    InstanceMethod("toString", &Atom::value),
  });
  exports.Set("Atom", atom);
  return exports;
}
Tuple::Tuple(const Napi::CallbackInfo &info)  : Napi::ObjectWrap<Tuple>(info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  if(info.Length() <= 0) {
    Napi::TypeError::New(env, "Tuple constructor requires value").ThrowAsJavaScriptException();
  }

  if(!info[0].IsArray()) {
    Napi::TypeError::New(env, "Array must be passed to constructor").ThrowAsJavaScriptException();
  }
  this->array = Napi::Persistent(info[0].As<Napi::Array>());
}

Napi::FunctionReference Tuple::constructor;
Napi::Object Tuple::Exports(Napi::Env env, Napi::Object exports) {
  Napi::Function tuple = DefineClass(env, "Tuple", {
    InstanceAccessor("value", &Tuple::value, NULL),
  });
  constructor = Napi::Persistent(tuple);
  constructor.SuppressDestruct();
  exports.Set("Tuple", tuple);
  return exports;
}
