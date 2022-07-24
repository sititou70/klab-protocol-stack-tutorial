SRC=src

APPS = 

DRIVERS = \
	$(SRC)/driver/dummy.o \
	$(SRC)/driver/loopback.o \

OBJS = \
	$(SRC)/util.o \
	$(SRC)/net.o \
	$(SRC)/ip.o \

TESTS = \
	$(SRC)/test/step0.exe \
	$(SRC)/test/step1.exe \
	$(SRC)/test/step2.exe \
	$(SRC)/test/step3.exe \
	$(SRC)/test/step4.exe \
	$(SRC)/test/step5.exe \

CFLAGS := $(CFLAGS) -g -W -Wall -Wno-unused-parameter -I $(SRC)

ifeq ($(shell uname),Linux)
  # Linux specific settings
  BASE = $(SRC)/platform/linux
  CFLAGS := $(CFLAGS) -pthread -I $(BASE)
	OBJS := $(OBJS) \
		$(BASE)/intr.o
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
