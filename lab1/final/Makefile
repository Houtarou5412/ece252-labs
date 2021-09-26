# Makefile, ECE252  
# YOUNG HA AHN, BRIAN PENG

CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz   # link with libz

# For students 
LIB_UTIL = zutil.o crc.o
SRCS   = pnginfo.c findpng.c catpng.c crc.c zutil.c
OBJS   = pnginfo.c findpng.c catpng.c $(LIB_UTIL) 

TARGETS= pnginfo findpng catpng

all: ${TARGETS}

pnginfo: pnginfo.c $(LIB_UTIL)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

findpng: findpng.c $(LIB_UTIL)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

catpng:  catpng.c $(LIB_UTIL)
 	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)



.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
