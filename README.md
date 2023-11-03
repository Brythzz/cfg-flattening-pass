# CFG Flattening LLVM Pass
Control-flow Graph Flattening pass for LLVM 17

Build the pass
```sh
$ make pass
```

Run the pass
```sh
$ clang -fpass-plugin=`echo build/pass/FlattenCFGPass.*` something.c
```