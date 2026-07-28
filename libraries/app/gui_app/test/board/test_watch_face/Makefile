# ============================================================================
# Makefile for Board Test: Watch Face GUI on OLED (PULPissimo RISC-V)
# Location: gui_app/test/board/
#
# Usage:
#   source /path/to/pulp-runtime/configs/pulpissimo.sh
#   make clean all run platform=fpga
# ============================================================================

PULP_APP = watch_face_test

# Paths relative to this Makefile
OLED_DIR   = ../../../../../sensors/OLED
GUI_DIR    = $(OLED_DIR)/GUI
UGUI_DIR   = $(GUI_DIR)/ugui
UI_DIR     = $(UGUI_DIR)/ui
APP_DIR    = ../../..

PULP_APP_C_SRCS = \
	main.c \
	$(APP_DIR)/gui_manager.c \
	$(APP_DIR)/screens/watch_face.c \
	$(OLED_DIR)/oled.c \
	$(OLED_DIR)/font/font5x7.c \
	$(GUI_DIR)/gui_port.c \
	$(GUI_DIR)/gesture.c \
	$(UGUI_DIR)/ugui.c \
	$(UI_DIR)/Fonts/font_6x10.c \
	$(UI_DIR)/ugui_button.c \
	$(UI_DIR)/ugui_checkbox.c \
	$(UI_DIR)/ugui_image.c \
	$(UI_DIR)/ugui_progress.c \
	$(UI_DIR)/ugui_textbox.c

PULP_CFLAGS = -Os -flto -g \
	-I$(OLED_DIR) \
	-I$(GUI_DIR) \
	-I$(UGUI_DIR) \
	-I$(UI_DIR) \
	-I$(APP_DIR)

include $(PULPRT_HOME)/rules/pulp.mk
