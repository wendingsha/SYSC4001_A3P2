CC = gcc
CFLAGS = -Wall -g

all: Part2_A Part2_B

Part2_A: Part2_A_101186335_101139708.c
	$(CC) $(CFLAGS) -o Part2_A Part2_A_101186335_101139708.c

Part2_B: Part2_B_101186335_101139708.c
	$(CC) $(CFLAGS) -o Part2_B Part2_B_101186335_101139708.c

clean:
	rm -f Part2_A Part2_B
