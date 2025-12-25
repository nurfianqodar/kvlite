CC 		:= clang
CFLAGS	:= -Wall -g -O2
CLIBS	:= -luring -ljemalloc
TARGET 	:= kvlite
SRCS	:= kvlite.c io.c util.c
OBJS	:= $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(CLIBS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)
