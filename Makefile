TARGET=main.exe
CC=gcc
DEBUG=-g
OPT=-O0
WARN=-Wall
PTHREAD=-pthread
FLAGS=$(DEBUG) $(PTHREAD) $(OPT) $(WARN)
SOURCE_CODE=main.c

install:
	$(CC) $(FLAGS) $(SOURCE_CODE) -o $(TARGET)
