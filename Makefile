FILES = main.c fifo.c dsa.c logger.c ecs.c render.c engine.c gamecomp.c mathutils.c
PROGRAM = ./program.exe

build:
	gcc $(FILES) -o $(PROGRAM) -g -lraylib -lopengl32 -lgdi32 -lwinmm

build_run: build
	$(PROGRAM)