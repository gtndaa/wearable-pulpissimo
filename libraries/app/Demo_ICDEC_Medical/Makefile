PULP_APP = demo_medical_watch

PULP_APP_C_SRCS = \
    main.c \
    ../../sensors/DS1307/rtc.c \
    ../../sensors/max30102/max30102.c \
    ../../sensors/max30102/filter/ppg_hr.c \
    ../../sensors/OLED/oled.c \
    ../../sensors/OLED/GUI/ugui/ugui.c \
    ../../sensors/OLED/GUI/ugui/ui/Fonts/font_8x8.c \
    ../../sensors/OLED/GUI/ugui/ui/Fonts/font_12x16.c \
    ../../sensors/OLED/GUI/ugui/ui/Fonts/font_6x8.c \
    ../../sensors/OLED/GUI/gesture.c \
    ../../sensors/OLED/GUI/gui_port.c

PULP_CFLAGS = -O3 -g \
    -I. \
    -I../../sensors/DS1307 \
    -I../../sensors/max30102 \
    -I../../sensors/max30102/filter \
    -I../../sensors/OLED \
    -I../../sensors/OLED/GUI \
    -I../../sensors/OLED/GUI/ugui \
    -I../../sensors/OLED/GUI/ugui/ui

include $(PULPRT_HOME)/rules/pulp.mk
