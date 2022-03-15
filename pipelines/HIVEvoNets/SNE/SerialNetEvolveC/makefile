#
#make file for treevolve
#

OFILES = const.o \
	 coseq.o \
	 exp.sub.o \
	 mathfuncs.o \
	 nodestack.o \
	 treevolve.o \
	 const.sub.o \
	 exp.o \
	 Heapsort.o \
	 models.o \
#	 twister.o \

CC = gcc

LDFLAGS = -lm

%.o: %.c %.h
	$(CC) -c $(CPFLAGS) $*.c

%.c: %.h

%.h:

snetevolve: $(OFILES)
	$(CC) -o snetevolve $(OFILES) -lm

clean:	rm *.o
