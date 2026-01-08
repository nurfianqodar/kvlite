CC		:= clang
CFLAGS	:= -Wall -Wextra -O3
CLIBS	:= -luring -ljemalloc

TARGET	:= kvlite
SRCS	:= kvlite.c
OBJS	:= $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(CLIBS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -rf $(TARGET) $(OBJS)
