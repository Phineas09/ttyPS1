CFLAGS=-g -Wall -O3

APPLICATIONS=example example2 argParse	

all:    $(APPLICATIONS)

%:      %.c
		gcc  $(CFLAGS) -pthread $@.c -o $@

clean:
		rm -f *~ $(APPLICATIONS)
