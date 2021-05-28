PORT = 57052
CFLAGS= -DPORT=\$(PORT) -g -Wall
DEPENDENCIES = xmodemserver.h crc16.h helper.h
all : xmodemserver client
xmodemserver : xmodemserver.o crc16.o helper.o
	gcc ${CFLAGS} -o $@ $^
	
client : client1.o crc16.o
	gcc ${CFLAGS} -o $@ $^
	
%.o : %.c ${DEPENDENCIES}
	gcc ${CFLAGS} -c $<
	
clean :
	rm -f *.o xmodemserver client
