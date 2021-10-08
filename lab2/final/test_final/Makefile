
# Makefile, ECE252  
# YOUNG HA AHN, BRIAN PENG

CC = gcc
CFLAGS = -Wall -g -std=c99 
LD = gcc
LDFLAGS = -g -std=c99
LDLIBS = -pthread -lcurl -lz

LIB_UTIL = zutil.o crc.o 
SRCS   = paster.c catpng.c crc.c zutil.c main_writer_header_cb.c
OBJS   = paster.c

TARGETS= paster

all: ${TARGETS}

paster: paster.c main_writer_header_cb.c $(LIB_UTIL) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) 
	rm all.png
	rm paster
