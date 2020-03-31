CC          = g++
CFLAGS      = -Wall -std=c++11

DIR_RECVFILE  = ./recvfile
DIR_SENDFILE  = ./sendfile


all: recvfile sendfile

recvfile:
	$(CC) $(CFLAGS) -o recvfile recvfile.cc

sendfile:
	$(CC) $(CFLAGS) -o sendfile sendfile.cc

clean:
	rm -f *.o
	rm -f *~
	rm -f *.gch
	rm -f recvfile
	rm -f sendfile
