OBJS	= sut.o test1.o
SOURCE	= sut.c test1.c
HEADER	= sut.h queue.h
OUT	= test1.out
CC	 = gcc
FLAGS	 = -g3 -c -Wall
LFLAGS	 = -lpthread
# -g option enables debugging mode 
# -c flag generates object code for separate files


all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)


# create/compile the individual files >>separately<<
sut.o: sut.c
	$(CC) $(FLAGS) sut.c 

test1.o: test1.c
	$(CC) $(FLAGS) test1.c 


# clean house
clean:
	rm -f $(OBJS) $(OUT)

# run the program
run: $(OUT)
	./$(OUT)

# compile program with debugging information
debug: $(OUT)
	valgrind $(OUT)

# run program with valgrind for errors
valgrind: $(OUT)
	valgrind $(OUT)

# run program with valgrind for leak checks
valgrind_leakcheck: $(OUT)
	valgrind --leak-check=full $(OUT)

# run program with valgrind for leak checks (extreme)
valgrind_extreme: $(OUT)
	valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes --vgdb=yes $(OUT)

