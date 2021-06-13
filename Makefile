CFLAGS=-g -Wall -O3

APPLICATIONS=pmodbt

all:    $(APPLICATIONS)

%:      %.c
		gcc  $(CFLAGS) -pthread $@.c -o $@

clean:
		rm -f *~ $(APPLICATIONS)
