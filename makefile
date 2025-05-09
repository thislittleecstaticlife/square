# makefile : square

target = square
cc = gcc
cflags = -Wall -Wextra -Wpedantic -std=c23
lflags = -lvulkan -ltiff
objects = square.o utilities.o 
shaders = vertex.spv fragment.spv

$(target): $(objects)
	$(cc) -o $(target) $(cflags) $(lflags) $(objects)

%.o:
	$(cc) -c -o $@ $(cflags) $<

%.spv:
	glslc -o $@ $<

square.o: square.c utilities.h $(shaders)
utilities.o: utilities.c utilities.h

vertex.spv: vertex.glsl
fragment.spv: fragment.glsl

.PHONY: test
test: $(target)
	rm -f output.*
	./$(target)

.PHONY: clean
clean:
	rm -f $(target) *.o *.spv output.*

