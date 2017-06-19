
CC=gcc
CFLAGS=-std=gnu99 -Wall -g -Wextra -pedantic -pthread -O

proj2:
	$(CC) $(CFLAGS) -o proj2 proj2.c

clean:
	rm -f proj2 core*
