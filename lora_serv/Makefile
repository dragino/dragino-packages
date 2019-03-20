APP_NAME:=ns

NSOBJECTS:=obj/$(APP_NAME).o obj/base64.o obj/parson.o obj/aes.o obj/cmac.o obj/utilities.o obj/LoRaMacCrypto.o obj/generic_list.o obj/db.o obj/handle.o

ASOBJECTS:=obj/as.o obj/base64.o obj/parson.o obj/aes.o obj/cmac.o obj/utilities.o obj/LoRaMacCrypto.o obj/generic_list.o obj/db.o obj/handle.o

NCOBJECTS:=obj/nc.o obj/base64.o obj/parson.o obj/aes.o obj/cmac.o obj/utilities.o obj/LoRaMacCrypto.o obj/generic_list.o obj/db.o obj/handle.o

CFLAGS := -Iinc

LIBS := -lsqlite3 -lpthread

all: $(APP_NAME) as nc

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME) as nc

obj:
	mkdir -p obj

obj/base64.o: src/base64.c inc/base64.h
	gcc -c $(CFLAGS)  $< -o $@ 

obj/parson.o: src/parson.c inc/parson.h
	gcc -c $(CFLAGS) $< -o $@

obj/aes.o: src/aes.c inc/aes.h
	gcc -c $(CFLAGS) $< -o $@

obj/cmac.o: src/cmac.c inc/cmac.h
	gcc -c $(CFLAGS) $< -o $@

obj/utilities.o: src/utilities.c inc/utilities.h
	gcc -c $(CFLAGS) $< -o $@

obj/LoRaMacCrypto.o: src/LoRaMacCrypto.c inc/LoRaMacCrypto.h inc/aes.h inc/cmac.h inc/utilities.h
	gcc -c $(CFLAGS) $< -o $@

obj/generic_list.o: src/generic_list.c inc/generic_list.h
	gcc -c $(CFLAGS) $< -o $@

obj/db.o: src/db.c inc/db.h
	gcc -c $(CFLAGS) $< -o $@ 

obj/handle.o: src/handle.c inc/handle.h inc/LoRaMacCrypto.h  inc/parson.h
	gcc -c $(CFLAGS) $< -o $@ 

obj/as.o: src/as.c inc/handle.h inc/base64.h inc/parson.h 
	gcc -c $(CFLAGS) $< -o $@ 

obj/nc.o: src/nc.c inc/handle.h inc/base64.h inc/parson.h 
	gcc -c $(CFLAGS) $< -o $@ 

obj/$(APP_NAME).o: src/ns.c inc/handle.h inc/base64.h inc/parson.h 
	gcc -c $(CFLAGS) $< -o $@ 

as: $(ASOBJECTS)
	gcc  $(ASOBJECTS) -o $@ $(LIBS)

nc: $(NCOBJECTS)
	gcc  $(NCOBJECTS) -o $@ $(LIBS)

$(APP_NAME): $(NSOBJECTS)
	gcc  $(NSOBJECTS) -o $@ $(LIBS)
