CC = gcc
CFLAGS = -Wall -Wextra -g

main: main.c
	$(CC) $(CFLAGS) main.c -o main.out

run: main
	./main.out

clean:
	rm main.out