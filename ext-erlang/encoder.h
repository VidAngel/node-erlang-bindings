#pragma once
#include <napi.h>
#include <erl_interface.h>
#include <ei.h>
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term, int* index);
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term);
void encode_value(Napi::Env env, Napi::Value val, ei_x_buff* request);

bool is_tuple(Napi::Value val);
bool is_atom(Napi::Value val); 
template <typename T> 
T* unwrap(Napi::Value val) {
  return Napi::ObjectWrap<T>::Unwrap(val.As<Napi::Object>());
}
