include GNUmakevars

LIB	= -lant

table:	table.o
	$(CC) -o table table.o $(LIB)

install: $(BINDIR)/table $(MAN1DIR)/table.1
clean:
	rm -f *.o table core a.out

$(BINDIR)/table: table
	cp table $(BINDIR)/table

$(MAN1DIR)/table.1: table.1
	cp table.1 $(MAN1DIR)/table.1
