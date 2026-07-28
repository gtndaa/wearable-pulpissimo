PULP_APP = test
PULP_APP_C_SRCS = \
	main.c \
	tests/test_pixel.c \
	tests/test_line.c \
	tests/test_kotak.c \
	tests/test_tangga.c \
	tests/test_papancatur.c \
	tests/test_text.c \
	tests/test_circle.c \
	font/font5x7.c \
	oled.c \
	GUI/gui_port.c \
	GUI/gesture.c \
	GUI/ugui/ugui.c \
	GUI/ugui/ui/ugui_button.c \
	GUI/ugui/ui/ugui_checkbox.c \
	GUI/ugui/ui/ugui_image.c \
	GUI/ugui/ui/ugui_progress.c \
	GUI/ugui/ui/ugui_textbox.c \
	GUI/ugui/ui/Fonts/font_6x10.c

PULP_CFLAGS = -Os -flto -g -IGUI -IGUI/ugui -IGUI/ugui/ui

include $(PULPRT_HOME)/rules/pulp.mk

