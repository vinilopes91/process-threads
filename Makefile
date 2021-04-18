TARGET=main
CC=gcc
DEBUG=-g
OPT=-O0
WARN=-Wall
PTHREAD=-pthread
FLAGS=$(DEBUG) $(PTHREAD) $(OPT) $(WARN)

install:
	$(CC) $(FLAGS) main.c -o $(TARGET)
