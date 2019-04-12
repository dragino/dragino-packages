APP_NAME:=lora_serv

OBJDIR = obj

CFLAGS := -Iinc -DBIGENDIAN -DLG08_LG02

LIBS := -lsqlite3 -lpthread

LIBNOP := -lsqlite3 

all: $(APP_NAME) lw_utili

OBJECTS:=$(OBJDIR)/$(APP_NAME).o $(OBJDIR)/base64.o $(OBJDIR)/parson.o $(OBJDIR)/aes.o $(OBJDIR)/cmac.o $(OBJDIR)/utilities.o $(OBJDIR)/LoRaMacCrypto.o $(OBJDIR)/generic_list.o $(OBJDIR)/db.o $(OBJDIR)/handle.o

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME) lw_utili

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/base64.o: src/base64.c inc/base64.h | $(OBJDIR)
	$(CC) -c $(CFLAGS)  $< -o $@ 

$(OBJDIR)/parson.o: src/parson.c inc/parson.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/aes.o: src/aes.c inc/aes.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/cmac.o: src/cmac.c inc/cmac.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/utilities.o: src/utilities.c inc/utilities.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/LoRaMacCrypto.o: src/LoRaMacCrypto.c inc/LoRaMacCrypto.h inc/aes.h inc/cmac.h inc/utilities.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/generic_list.o: src/generic_list.c inc/generic_list.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/db.o: src/db.c inc/db.h inc/handle.h inc/utilities.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@ 

$(OBJDIR)/handle.o: src/handle.c inc/handle.h inc/LoRaMacCrypto.h inc/parson.h inc/db.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@ 

$(OBJDIR)/lw_utili.o: src/lw_utili.c inc/lw_utili.h | $(OBJDIR) 
	$(CC) -c $(CFLAGS) $< -o $@ 

$(OBJDIR)/$(APP_NAME).o: src/$(APP_NAME).c inc/handle.h inc/db.h inc/base64.h inc/parson.h  | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@ 

lw_utili: $(OBJDIR)/lw_utili.o
	$(CC) $(OBJDIR)/lw_utili.o -o $@ $(LIBNOP)

$(APP_NAME): $(OBJECTS)
	$(CC)  $(OBJECTS) -o $@ $(LIBS)
