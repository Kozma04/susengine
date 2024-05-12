FILES = main.c fifo.c dsa.c logger.c ecs.c render.c engine.c gamecomp.c mathutils.c physics.c
PROGRAM = ./program

build:
	gcc $(FILES) -o $(PROGRAM) $(RAYLIB_PARAMS) -g -lraylib -lGL -lm -lpthread -ldl -lrt -lc -lX11

build_run: build
	$(PROGRAM)