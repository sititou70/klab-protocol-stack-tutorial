SRC=src

APPS =

DRIVERS = \
	$(SRC)/driver/dummy.o \
	$(SRC)/driver/loopback.o \

OBJS = \
	$(SRC)/tcp.o \
	$(SRC)/udp.o \
	$(SRC)/arp.o \
	$(SRC)/icmp.o \
	$(SRC)/ip.o \
	$(SRC)/net.o \
	$(SRC)/ether.o \
	$(SRC)/util.o \

TESTS = \
	$(SRC)/test/step0.exe \
	$(SRC)/test/step1.exe \
	$(SRC)/test/step2.exe \
	$(SRC)/test/step3.exe \
	$(SRC)/test/step4.exe \
	$(SRC)/test/step5.exe \
	$(SRC)/test/step6.exe \
	$(SRC)/test/step7.exe \
	$(SRC)/test/step8.exe \
	$(SRC)/test/step9.exe \
	$(SRC)/test/step10.exe \
	$(SRC)/test/step11.exe \
	$(SRC)/test/step12.exe \
	$(SRC)/test/step13.exe \
	$(SRC)/test/step14.exe \
	$(SRC)/test/step15.exe \
	$(SRC)/test/step16.exe \
	$(SRC)/test/step17.exe \
	$(SRC)/test/step18.exe \
	$(SRC)/test/step19.exe \
	$(SRC)/test/step20-1.exe \
	$(SRC)/test/step20-2.exe \
	$(SRC)/test/step21.exe \
	$(SRC)/test/step22.exe \
	$(SRC)/test/step23.exe \
	$(SRC)/test/step24.exe \
	$(SRC)/test/step25.exe \
	$(SRC)/test/step27.exe \
	$(SRC)/test/step28.exe \
	$(SRC)/test/simple-http.exe \
	$(SRC)/test/static-http-server.exe \

CFLAGS := $(CFLAGS) -g -W -Wall -Wno-unused-parameter -I $(SRC)

ifeq ($(shell uname),Linux)
  # Linux specific settings
  BASE = $(SRC)/platform/linux

  CFLAGS := $(CFLAGS) -pthread -I $(BASE)
	LDFLAGS := $(LDFLAGS) -lrt

	DRIVERS := \
		$(DRIVERS) \
		$(BASE)/driver/ether_tap.o \

	OBJS := \
		$(OBJS) \
		$(BASE)/intr.o \
		$(BASE)/sched.o \

endif

ifeq ($(shell uname),Darwin)
  # macOS specific settings
endif

.SUFFIXES:
.SUFFIXES: .c .o

.PHONY: all clean

all: $(APPS) $(TESTS)

$(APPS): %.exe : %.o $(OBJS) $(DRIVERS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TESTS): %.exe : %.o $(OBJS) $(DRIVERS) $(SRC)/test/test.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(APPS) $(APPS:.exe=.o) $(OBJS) $(DRIVERS) $(TESTS) $(TESTS:.exe=.o)
