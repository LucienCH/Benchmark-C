OBJS	= benchmark.o
SOURCE	= benchmark.c
HEADER	= 
OUT	= benchmark
CC	 = gcc
FLAGS	 = -g -c -Wall
LFLAGS	 = -lm

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

benchmark.o: benchmark.c
	$(CC) $(FLAGS) benchmark.c 


clean:
	rm -f $(OBJS) $(OUT)
