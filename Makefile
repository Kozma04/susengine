FILES = *.c engine/*.c utils/*.c
CLANG_FORMAT_TARGETS = *.c engine/*.c *.h engine/*.h utils/*.c utils/*.h
PROGRAM = ./program

format:
	clang-format -i --style=LLVM --style="{BasedOnStyle: llvm, IndentWidth: 4}" $(CLANG_FORMAT_TARGETS)

build: format
	gcc $(FILES) -o $(PROGRAM) $(RAYLIB_PARAMS) -D_GLFW_WAYLAND -DUSE_WAYLAND=ON -g -fpermissive -llua -lraylib -lGL -lm -lpthread -ldl -lrt -lc

build_run: build
	$(PROGRAM)