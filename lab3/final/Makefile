
# Makefile, ECE252  
# YOUNG HA AHN, BRIAN PENG
# LAB3 Brian and Jason

CC = gcc
CFLAGS = -Wall -g -std=c99 
LD = gcc
LDFLAGS = -g -std=c99
LDLIBS = -pthread -lcurl -lz

LIB_UTIL = zutil.o crc.o 
SRCS   = paster2.c crc.c zutil.c
OBJS   = paster2.c

TARGETS= paster2

all: ${TARGETS}

paster2: paster2.c $(LIB_UTIL) 
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
