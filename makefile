CC		:= clang
CFLAGS	:= -Wall -Wextra -O3 -g
CLIBS	:= -luring -ljemalloc

TARGET	:= kvlite
SRCS	:= kvlite.c util.c task.c server.c
OBJS	:= $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(CLIBS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -rf $(TARGET) $(OBJS)
