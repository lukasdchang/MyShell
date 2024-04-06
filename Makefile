CC = gcc
CFLAGS = -Wall -Wextra

mysh: mysh.o parser.o executor.o
	$(CC) $(CFLAGS) -o mysh mysh.o parser.o executor.o

mysh.o: mysh.c
	$(CC) $(CFLAGS) -c mysh.c

parser.o: parser.c
	$(CC) $(CFLAGS) -c parser.c

executor.o: executor.c
	$(CC) $(CFLAGS) -c executor.c

clean:
	rm -f mysh *.o
