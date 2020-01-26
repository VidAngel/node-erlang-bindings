{"targets": [
  {
    "target_name": "erlang",
    "sources": [
      "ext-erlang/encoder.cc",
      "ext-erlang/erlang-node.cc",
      "ext-erlang/exports.cc"
    ],
    "include_dirs": ["<!@(node -p \"require('node-addon-api').include\")"],
    "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
    "cflags": [
      "-I<!(./root-dir.erl)/usr/include/"
    ],
    "cflags_cc": [
      "-I<!(./root-dir.erl)/usr/include/"
    ],
    "ldflags": [
      "-L<!(./root-dir.erl)/usr/lib"
    ],
    "libraries": [
      "-lpthread",
      "-lei"
    ],
    "cflags!": ["-fno-exceptions"],
    "cflags_cc!": ["-fno-exceptions"],
    "xcode_settings": {
      "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
      "CLANG_CXX_LIBRARY": "libc++",
      "MACOSX_DEPLOYMENT_TARGET": "10.7",
      "OTHER_CFLAGS": [
        "-I<!(./root-dir.erl)/usr/include/"
      ],
      "OTHER_CFLAGS_CC": [
        "-I<!(./root-dir.erl)/usr/include/"
      ],
      "OTHER_LDFLAGS": [
        "-L<!(./root-dir.erl)/usr/lib"
      ],
    },
    "msvs_settings": {
      "VCCLCompilerTool": { "ExceptionHandling": 1 },
    },
    "conditions": [['OS=="mac"', {
      'cflags+': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES', # -fvisibility=hidden
      }
    }]]
  }
]}
