CC = gcc
CFLAGS = -Wall -Wextra -g

main.out: main.c
	$(CC) $(CFLAGS) main.c -o main.out

run: main.out
	./main.out

clean:
	rm main.out
