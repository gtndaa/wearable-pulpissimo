# ============================================================================
# Makefile for GUI Application Layer — Linux PC Simulation & Unit Test
# Location: gui_app/test/sim/
# ============================================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter

# Path to friend's GUI framework (read-only, no modifications)
GUI_ROOT = ../../../../sensors/OLED/GUI
UGUI_DIR = $(GUI_ROOT)/ugui
UI_DIR   = $(UGUI_DIR)/ui

# Path to our application modules
APP_DIR  = ../..

# Include paths
INCLUDES = -I$(UGUI_DIR) -I$(UI_DIR) -I$(GUI_ROOT) -I$(APP_DIR)

# Target executable
TARGET = test_gui_app

# Source files
SRCS = test_gui_app.c \
       $(APP_DIR)/screens/watch_face.c \
       $(UGUI_DIR)/ugui.c \
       $(UI_DIR)/Fonts/font_6x10.c \
       $(UI_DIR)/ugui_button.c \
       $(UI_DIR)/ugui_checkbox.c \
       $(UI_DIR)/ugui_image.c \
       $(UI_DIR)/ugui_progress.c \
       $(UI_DIR)/ugui_textbox.c \
       $(GUI_ROOT)/gesture.c

OBJS = $(SRCS:.c=.o)

# ============================================================================
.PHONY: all test sim clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TARGET)
	./$(TARGET)

sim: $(TARGET)
	./$(TARGET) --sim

clean:
	rm -f $(OBJS) $(TARGET)
