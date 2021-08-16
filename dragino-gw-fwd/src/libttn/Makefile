PAHO_SRC = ../libmqtt
OBJDIR = obj
RM = rm -f
NAME = ttn-gateway-connector
TARGET_LIB = lib$(NAME).so

CC ?= gcc

SRCS = *.c

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

CFLAGS_SO := $(CFLAGS) -fPIC -shared -O2 -Wall -I$(PAHO_SRC)
LDD = -lpthread -L$(PAHO_SRC) -lpahomqtt3c

all: $(OBJDIR)/$(TARGET_LIB) test 

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/$(TARGET_LIB): $(SRCS) | $(OBJDIR)
	$(CC) $(CFLAGS_SO) -o $@ $^ $(LDD)

.PHONY: test
test: $(OBJDIR)/$(NAME)_test

$(OBJDIR)/$(NAME)_test: $(OBJDIR)/$(TARGET_LIB)
	$(CC) -fPIC -Wall -g -O2 -I. test/test.c -o $@ $(LDD) -L$(OBJDIR) -l$(NAME) -lprotobuf-c 

.PHONY: clean
clean:
	-$(RM) $(OBJDIR)/??*
