# CFG Flattening LLVM Pass
Control-flow Graph Flattening pass for LLVM 17

Build the pass
```sh
$ mkdir build
$ cd build
$ cmake ..
$ make
$ cd ..
```

Run the pass
```sh
$ clang -fpass-plugin=`echo build/pass/FlattenCFGPass.*` something.c
```