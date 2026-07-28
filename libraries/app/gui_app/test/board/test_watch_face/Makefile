# ============================================================================
# Makefile for Board Test: Watch Face on OLED with RTC (PULPissimo RISC-V)
# Location: gui_app/test/board/test_watch_face/
#
# Template mengikuti Demo_ICDEC_Medical/Makefile
#
# Usage:
#   source build.sh  (or manual: source pulpissimo.sh && make clean all)
#   make run platform=fpga
# ============================================================================

PULP_APP = watch_face_test

# Paths relative to this Makefile (inside test/board/test_watch_face/)
SENSORS_DIR = ../../../../../sensors
OLED_DIR    = $(SENSORS_DIR)/OLED
RTC_DIR     = $(SENSORS_DIR)/DS1307

PULP_APP_C_SRCS = \
    main.c \
    $(RTC_DIR)/rtc.c \
    $(OLED_DIR)/oled.c \
    $(OLED_DIR)/GUI/ugui/ugui.c \
    $(OLED_DIR)/GUI/ugui/ui/Fonts/font_8x8.c \
    $(OLED_DIR)/GUI/ugui/ui/Fonts/font_12x16.c \
    $(OLED_DIR)/GUI/ugui/ui/Fonts/font_6x8.c \
    $(OLED_DIR)/GUI/gesture.c \
    $(OLED_DIR)/GUI/gui_port.c

PULP_CFLAGS = -O3 -g \
    -I. \
    -I$(RTC_DIR) \
    -I$(OLED_DIR) \
    -I$(OLED_DIR)/GUI \
    -I$(OLED_DIR)/GUI/ugui \
    -I$(OLED_DIR)/GUI/ugui/ui

include $(PULPRT_HOME)/rules/pulp.mk
