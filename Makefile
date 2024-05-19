FILES = *.c engine/*.c
PROGRAM = ./program

build:
	gcc $(FILES) -o $(PROGRAM) $(RAYLIB_PARAMS) -D_GLFW_WAYLAND -DUSE_WAYLAND=ON -g -fpermissive -lraylib -lGL -lm -lpthread -ldl -lrt -lc

build_run: build
	$(PROGRAM)