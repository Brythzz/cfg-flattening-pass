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
# 1 iteration (default)
$ clang -fpass-plugin=`echo build/pass/FlattenCFGPass.*` something.c

# 3 iterations
$ clang -fplugin=`echo build/pass/FlattenCFGPass.*` -fpass-plugin=`echo build/pass/FlattenCFGPass.*` something.c -mllvm -iterations=3
```