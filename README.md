SYSC 4001 - Assignment 3 Part 2
Students: 101186335 and 101139708

## How to Compile
Included a Makefile so you can run `make` in the terminal to build everything.

If you want to compile them manually you can do:
gcc -o Part2_A Part2_A_101186335_101139708.c
gcc -o Part2_B Part2_B_101186335_101139708.c

## How to Run
You need to pass the number of TAs you want as an argument.

For Part A (No Semaphores):
./Part2_A 2

For Part B (With Semaphores):
./Part2_B 2

(You can change 2 to however many TAs you want to test with, like 5 or 10).

## Setup
Make sure the "exams" folder is there and has the text files in it (exam_01.txt to exam_20.txt).
Also make sure rubric.txt is in the same folder.

## Notes
Part A uses shared memory but doesn't really protect it, so race conditions happen.
Part B adds semaphores (sem 0 for rubric, sem 1 for exams) to fix the race conditions.
