{
  "targets": [
    {
      "target_name": "node-lmdb",
      "sources": [
        "dependencies/lmdb/libraries/liblmdb/mdb.c",
        "dependencies/lmdb/libraries/liblmdb/midl.c",
        "src/node-lmdb.cpp",
        "src/env.cpp",
        "src/misc.cpp",
        "src/txn.cpp",
        "src/dbi.cpp",
        "src/cursor.cpp"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "dependencies/lmdb/libraries/liblmdb"
      ],
      "conditions": [
        ["OS=='linux'", {
          "ldflags": [
            "-O3",
            "-rdynamic"
          ],
          "cflags": [
            "-fPIC",
            "-O3"
          ],
          "cflags_cc": [
            "-fvisibility-inlines-hidden",
            "-std=c++0x"
          ]
        }],
        ["OS=='mac'", {
          "xcode_settings": {
            "OTHER_CPLUSPLUSFLAGS" : ["-std=c++11","-mmacosx-version-min=10.7"],
            "OTHER_LDFLAGS": ["-std=c++11"],
          }
        }],
        ["OS=='win'", {
          "win_delay_load_hook": "false",
          "libraries": [ "../dependencies/wdk/x64/ntdll.lib" ]
        }]
      ]
    }
  ]
}
