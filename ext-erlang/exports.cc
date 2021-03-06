#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <napi.h>
#include "./custom-types.h"
#include "./erlang-node.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  ErlangNode::Exports(env, exports);
  Atom::Exports(env, exports);
  Tuple::Exports(env, exports);
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
