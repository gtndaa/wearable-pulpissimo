# Compiler settings
CC = gcc
CFLAGS = -Wall -Wno-unused-function -I../ugui -I../ugui/ui
LDFLAGS =

# Target executable name
TARGET = test_gui_input

# Source files
SRCS = test_gui_input.c \
       ../ugui/ugui.c \
       ../ugui/ui/ugui_button.c \
       ../ugui/ui/ugui_checkbox.c \
       ../ugui/ui/ugui_image.c \
       ../ugui/ui/ugui_progress.c \
       ../ugui/ui/ugui_textbox.c \
       ../gesture.c

# Object files (replace .c with .o)
OBJS = $(SRCS:.c=.o)

# Default rule: build the target
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule to compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the test
test: $(TARGET)
	./$(TARGET)

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET)
