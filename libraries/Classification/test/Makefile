PULP_APP = test

PULP_APP_C_SRCS = test.c \
                  ../classifier.c \
                  ../../sensors/mpu6050/mpu6050.c

PULP_LDFLAGS += -Wl,--start-group -lc -Wl,--end-group

PULP_CFLAGS = -O3 -g \
              -I.. \
              -I../../sensors/mpu6050

include $(PULPRT_HOME)/rules/pulp.mk
