CC	= gcc -Wall -O2
#CC	= ccc -Qwarn=4 
#CC	= cc -I/usr/local/include -O
BINDIR	= /usr/local/bin
MANDIR	= /usr/local/man/man1
LIB	= -lant
#CFLAGS	= -I../malloctrace
#LIB	= ../malloctrace/malloctrace.o

table:	table.o
	$(CC) -o table table.o $(LIB)

install: $(BINDIR)/table $(MANDIR)/table.1
clean:
	rm -f *.o table core a.out

$(BINDIR)/table: table
	cp table $(BINDIR)/table

$(MANDIR)/table.1: table.1
	cp table.1 $(MANDIR)/table.1
