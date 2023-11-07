gen_original_cfg: gen_llvm gen_cfg

gen_llvm: something.c
	clang -S -emit-llvm -Xclang -disable-O0-optnone something.c

gen_cfg: something.ll
	opt -p dot-cfg something.ll
	dot -Tpdf .main.dot -o main.pdf
	rm -f .*.dot

pass:
	@cd build && make && cd ..

passll:
	clang -fpass-plugin=`echo build/pass/FlattenCFGPass.*` something.c -emit-llvm -S -o something.ll

clean:
	rm -f .*.dot
	rm -f *.ll
	rm -f *.pdf

.PHONY: pass clean
