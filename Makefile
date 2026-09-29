CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -g

all: schedule demo test

schedule: schedule.c
	$(CC) $(CFLAGS) -o schedule schedule.c

demo: demo.c
	$(CC) $(CFLAGS) -o demo demo.c

test: test.c
	$(CC) $(CFLAGS) -o test test.c

clean:
	rm -f schedule demo test