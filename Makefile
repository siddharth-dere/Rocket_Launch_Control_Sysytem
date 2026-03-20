CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude
LDFLAGS = -lpthread -lrt

SRC_DIR = src
OBJ_DIR = obj
TARGET  = rocket_sim
CTRL    = control

SRCS    = $(SRC_DIR)/main.c $(SRC_DIR)/sensor.c $(SRC_DIR)/ipc.c
OBJS    = $(OBJ_DIR)/main.o $(OBJ_DIR)/sensor.o $(OBJ_DIR)/ipc.o

.PHONY: all clean run

all: $(OBJ_DIR) $(TARGET) $(CTRL)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  Built: $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(CTRL): tools/control.c $(OBJ_DIR)/ipc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  Built: $(CTRL)"

run: all
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(CTRL)
	rm -f /tmp/rocket_sim.log /tmp/rkt_cmd
	@echo "  Cleaned"
