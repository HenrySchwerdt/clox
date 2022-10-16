main:
	cc src/main.c src/chunk.c src/memory.c  src/value.c src/line_tracker.c src/vm.c src/utils.c src/debug.c src/scanner.c src/compiler.c -o build/clox