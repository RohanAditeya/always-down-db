# Variable to hold all object files.
objects = main.o

# Default make rule to build the final executable
main: $(objects)
	-mkdir target
	gcc -o target/main main.o

main.o: main.c
	gcc -o main.o -g -O0 -c main.c

# Clean rule to delete all object files and the final executable if present
# Intentioanlly added as prerequisite to .PHONY target to let make know that
# this rule must be executed even if a target file of name clean exists.
clean:
	-rm -R target main.o

.PHONY: clean