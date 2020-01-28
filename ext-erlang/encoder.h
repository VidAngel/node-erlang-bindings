#pragma once
#include <napi.h>
#include <erl_interface.h>
#include <ei.h>
Napi::Value decode_atom(Napi::Env env, ei_x_buff* term, int* index);
Napi::Number decode_int(Napi::Env env,ei_x_buff* term, int* index);
Napi::Number decode_double(Napi::Env env,ei_x_buff* term, int* index);
Napi::Object decode_map(Napi::Env env,ei_x_buff* term, int* index, int arity);
Napi::Object decode_tuple(Napi::Env env, ei_x_buff* term, int* index, int arity);
Napi::Array decode_list(Napi::Env env, ei_x_buff* term, int* index, int arity);
Napi::Value decode_binary(Napi::Env env, ei_x_buff* term, int* index, int size);
Napi::String decode_charlist(Napi::Env env, ei_x_buff* term, int* index, int size);
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term, int* index);
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term);

void encode_string(Napi::Env env, Napi::Value val, ei_x_buff* request);
void encode_atom(Napi::Env env, Napi::Value val, ei_x_buff* request);
void encode_tuple(Napi::Env env, Napi::Value val, ei_x_buff* request);
void encode_number(Napi::Env env, Napi::Value val, ei_x_buff* request);
void encode_object(Napi::Env env, Napi::Value val, ei_x_buff* request);
void encode_bool(Napi::Env env, Napi::Value val, ei_x_buff* request);
void encode_array(Napi::Env env, Napi::Value val, ei_x_buff* request);
void encode_value(Napi::Env env, Napi::Value val, ei_x_buff* request);

bool is_tuple(Napi::Value val);
bool is_atom(Napi::Value val); 
template <typename T> 
T* unwrap(Napi::Value val) {
  return Napi::ObjectWrap<T>::Unwrap(val.As<Napi::Object>());
}
