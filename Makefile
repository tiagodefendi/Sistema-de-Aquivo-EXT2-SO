CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
INCLUDES = -I. -Icommands

SRCS = ext2shell.c ext2_utils.c \
       commands/info.c commands/ls.c \
       commands/pwd.c commands/cd.c \
       commands/cat.c commands/attr.c \
	   commands/touch.c commands/mkdir.c \
	   commands/rm.c commands/rmdir.c \
	   commands/rename.c commands/cp.c \
	   commands/mv.c

OBJS = $(SRCS:.c=.o)
TARGET = ext2shell

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
