all:
	gcc -std=c99 compiler.c -o compiler
	./compiler program
	nasm -f elf64 interpreter.nasm -o interpreter.o
	ld interpreter.o -o interpreter
	./interpreter

clean:
	rm bytecode compiler *.o interpreter
