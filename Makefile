CC = gcc
CFLAGS = -O4 -Wall
DEST = /usr/local/bin
PROGRAM = onamae_ddns

$(PROGRAM): ddns.o
	$(CC) $(CFLAGS) -o $(PROGRAM) ddns.o

ddns.o: ddns.cpp
	$(CC) $(CFLAGS) -c ddns.cpp

install: $(PROGRAM)
	install -s $(PROGRAM) $(DEST)

clean:
	rm -f *.o *~ $(PROGRAM)
