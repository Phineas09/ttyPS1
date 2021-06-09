CFLAGS=-g -Wall -O3

APPLICATIONS=example example2	

all:    $(APPLICATIONS)

%:      %.c
		gcc  $(CFLAGS) $@.c -o $@

clean:
		rm -f *~ $(APPLICATIONS)
