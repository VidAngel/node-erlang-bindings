#ifndef NAPI_ENCODER
#define NAPI_ENCODER
#include <napi.h>
#include <erl_interface.h>
#include <ei.h>
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term, int* index);
Napi::Value decode_erlang(Napi::Env env, ei_x_buff* term);
void encode_value(Napi::Env env, Napi::Value val, ei_x_buff* request);
#endif
