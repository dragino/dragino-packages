### get external defined data

### constant symbols

ARCH ?=
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

CFLAGS_IN := -O2 -Wall -Wextra -std=c99 -Iinc -I.

OBJDIR = obj
INCLUDES = $(wildcard inc/*.h)

### linking options

### general build targets

all: libparson.a libbase64.a

clean:
	rm -f libparson.a
	rm -f libbase64.a
	rm -f $(OBJDIR)/*.o

### library module target

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(CFLAGS_IN) $< -o $@

### static library

libparson.a:  $(OBJDIR)/parson.o
	$(AR) rcs $@ $^

libbase64.a:  $(OBJDIR)/base64.o
	$(AR) rcs $@ $^

### test programs

### EOF
