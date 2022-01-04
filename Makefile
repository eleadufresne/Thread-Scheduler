OBJS	= sut.o
SOURCE	= sut.c
HEADER	= sut.h
OUT	= sut
CC	 = gcc
FLAGS	 = -g3 -c -Wall
LFLAGS	 =

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

sut.o: sut.c
	$(CC) $(FLAGS) sut.c


clean:
	rm -f $(OBJS) $(OUT)

run: $(OUT)
	./$(OUT)

debug: $(OUT)
	valgrind $(OUT)

valgrind: $(OUT)
	valgrind $(OUT)

valgrind_leakcheck: $(OUT)
	valgrind --leak-check=full $(OUT)

valgrind_extreme: $(OUT)
	valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes --vgdb=yes $(OUT)
