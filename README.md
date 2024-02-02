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

### Examples

Original CFG               |  One iteration            | Two iterations  
:-------------------------:|:-------------------------:|:-------------------------:
![](https://github.com/Brythzz/llvm-pass-cfg/assets/62302815/381a7b40-5d79-4b0f-8bf8-ba8bb7aab0bd) | ![](https://github.com/Brythzz/llvm-pass-cfg/assets/62302815/29bef239-bc48-46f0-8715-df13fbb6c41e) | ![](https://github.com/Brythzz/llvm-pass-cfg/assets/62302815/03febd34-98f5-4fc1-93ef-1b20c609d010)
