CC      := 	gcc
CFLAGS  := 	-Wall -g -Iinclude

SRC_DIR := 	src
CMD_DIR := 	commands
OBJ_DIR := 	.exec

SRCS    :=	$(SRC_DIR)/main.c $(SRC_DIR)/utils.c \
			$(CMD_DIR)/info.c $(CMD_DIR)/ls.c \
			$(CMD_DIR)/cat.c $(CMD_DIR)/pwd.c \
			$(CMD_DIR)/attr.c $(CMD_DIR)/cd.c \
			$(CMD_DIR)/touch.c $(CMD_DIR)/mkdir.c \
			$(CMD_DIR)/rm.c $(CMD_DIR)/rmdir.c \
			$(CMD_DIR)/rename.c $(CMD_DIR)/cp.c \
			$(CMD_DIR)/print.c

OBJS    := 	$(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(SRCS)))

TARGET  := 	ext2shell
IMG     := 	myext2image.img

.PHONY: all clean shell

all: $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(CMD_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

shell: all
	./$(TARGET) $(IMG)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)