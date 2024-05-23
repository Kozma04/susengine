FILES = *.c engine/*.c
CLANG_FORMAT_TARGETS = *.c engine/*.c *.h engine/*.h
PROGRAM = ./program

build:
	clang-format -i --style=LLVM --style="{BasedOnStyle: llvm, IndentWidth: 4}" $(CLANG_FORMAT_TARGETS)
	gcc $(FILES) -o $(PROGRAM) $(RAYLIB_PARAMS) -D_GLFW_WAYLAND -DUSE_WAYLAND=ON -g -fpermissive -lraylib -lGL -lm -lpthread -ldl -lrt -lc

build_run: build
	$(PROGRAM)