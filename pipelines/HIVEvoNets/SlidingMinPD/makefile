#
#make file for MinPD
#

OFILES = Heapsort.o \
	 mathfuncs.o \
	 MinPD.o \
	 NJ.o

CC = gcc

%.o: %.c %.h
	$(CC) -c $*.c

%.c: %.h

%.h:

minpd: $(OFILES)
	$(CC) -o minpd $(OFILES) -lm

clean:	rm *.o
