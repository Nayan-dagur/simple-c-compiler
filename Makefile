CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

TARGET = mycc
SRCS = main.c lexer.c parser.c compiler.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run-web: $(TARGET)
	./$(TARGET) --web
