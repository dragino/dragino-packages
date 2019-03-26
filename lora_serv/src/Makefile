APP_NAME:=lora_serv

OBJDIR = obj

CFLAGS := -Iinc

LIBS := -lsqlite3 -lpthread

all: $(APP_NAME)

OBJECTS:=$(OBJDIR)/$(APP_NAME).o obj/base64.o obj/parson.o obj/aes.o obj/cmac.o obj/utilities.o obj/LoRaMacCrypto.o obj/generic_list.o obj/db.o obj/handle.o

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/base64.o: src/base64.c inc/base64.h | $(OBJDIR)
	gcc -c $(CFLAGS)  $< -o $@ 

$(OBJDIR)/parson.o: src/parson.c inc/parson.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@

$(OBJDIR)/aes.o: src/aes.c inc/aes.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@

$(OBJDIR)/cmac.o: src/cmac.c inc/cmac.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@

$(OBJDIR)/utilities.o: src/utilities.c inc/utilities.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@

$(OBJDIR)/LoRaMacCrypto.o: src/LoRaMacCrypto.c inc/LoRaMacCrypto.h inc/aes.h inc/cmac.h inc/utilities.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@

$(OBJDIR)/generic_list.o: src/generic_list.c inc/generic_list.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@

$(OBJDIR)/db.o: src/db.c inc/db.h inc/handle.h inc/utilities.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@ 

$(OBJDIR)/handle.o: src/handle.c inc/handle.h inc/LoRaMacCrypto.h inc/parson.h inc/db.h | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@ 

$(OBJDIR)/$(APP_NAME).o: src/$(APP_NAME).c inc/handle.h inc/db.h inc/base64.h inc/parson.h  | $(OBJDIR)
	gcc -c $(CFLAGS) $< -o $@ 

$(APP_NAME): $(OBJECTS)
	gcc  $(OBJECTS) -o $@ $(LIBS)
