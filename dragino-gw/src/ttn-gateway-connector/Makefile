SRCDIR = src
OBJDIR = obj
BINDIR = bin
APIDIR = $(SRCDIR)/github.com/TheThingsNetwork/ttn/api
PAHO_SRC = ../paho.mqtt.embedded-c

CFLAGS_C = -fPIC -Wall -g -O2 -I$(SRCDIR) -I$(PAHO_SRC)/MQTTClient-C/src -I$(PAHO_SRC)/MQTTPacket/src -I$(SRCDIR)/github.com/gogo/protobuf/protobuf -I$(SRCDIR)/github.com/TheThingsNetwork -DMQTT_TASK

LDADD = -lpthread -L$(PAHO_SRC)/build/output/ -lpahomqtt -L$(PAHO_SRC) 
RM = rm -f
NAME = ttn-gateway-connector
TARGET_LIB = lib$(NAME).so

SRCS = $(SRCDIR)/connector.c $(SRCDIR)/../$(PAHO_SRC)/MQTTClient-C/src/MQTTClient.c $(SRCDIR)/github.com/gogo/protobuf/protobuf/google/protobuf/empty.pb-c.c $(APIDIR)/api.pb-c.c $(APIDIR)/protocol/protocol.pb-c.c $(APIDIR)/protocol/lorawan/lorawan.pb-c.c $(APIDIR)/gateway/gateway.pb-c.c $(APIDIR)/router/router.pb-c.c $(SRCDIR)/github.com/TheThingsNetwork/gateway-connector-bridge/types/types.pb-c.c $(SRCDIR)/github.com/TheThingsNetwork/ttn/api/trace/trace.pb-c.c

PROTOC = protoc-c --c_out=$(SRCDIR) --proto_path=$(GOPATH)/src -I$(GOPATH)/src/github.com/TheThingsNetwork -I$(GOPATH)/src/github.com/gogo/protobuf/protobuf $(GOPATH)/src

CFLAGS_C += -I$(PAHO_SRC)/MQTTClient-C/src/linux

SRCS += $(SRCDIR)/../$(PAHO_SRC)/MQTTClient-C/src/linux/MQTTLinux.c

OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: $(BINDIR)/$(TARGET_LIB) test

.PHONY: build
build: $(BINDIR)/$(TARGET_LIB)

$(BINDIR)/$(TARGET_LIB): $(OBJS) $(PAHO_SRC)/libpahomqtt.a
	mkdir -p $(BINDIR)
	$(CC) -shared $(LDFLAGS) -o $@ $^ $(LDADD)

$(OBJS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS_C) -c $< -o $@

.PHONY: proto
proto:
	$(PROTOC)/github.com/gogo/protobuf/protobuf/google/protobuf/descriptor.proto
	$(PROTOC)/github.com/gogo/protobuf/protobuf/google/protobuf/empty.proto
	$(PROTOC)/github.com/gogo/protobuf/gogoproto/gogo.proto
	$(PROTOC)/github.com/TheThingsNetwork/ttn/api/api.proto
	$(PROTOC)/github.com/TheThingsNetwork/ttn/api/trace/trace.proto
	$(PROTOC)/github.com/TheThingsNetwork/ttn/api/gateway/gateway.proto
	$(PROTOC)/github.com/TheThingsNetwork/ttn/api/protocol/protocol.proto
	$(PROTOC)/github.com/TheThingsNetwork/ttn/api/protocol/lorawan/lorawan.proto
	$(PROTOC)/github.com/TheThingsNetwork/ttn/api/router/router.proto
	$(PROTOC)/github.com/TheThingsNetwork/gateway-connector-bridge/types/types.proto

.PHONY: test
test: $(BINDIR)/$(NAME)_test

$(BINDIR)/$(NAME)_test: $(BINDIR)/$(TARGET_LIB)
	$(CC) -fPIC -Wall -g -O2 -I$(SRCDIR) -I$(SRCDIR)/github.com/gogo/protobuf/protobuf -I$(SRCDIR)/github.com/TheThingsNetwork src/test.c -o $@ -L$(BINDIR) -l$(NAME)

.PHONY: clean
clean:
	-$(RM) $(BINDIR)/$(TARGET_LIB) $(OBJS) $(BINDIR)/$(NAME)_test $(OBJDIR)/test.o
