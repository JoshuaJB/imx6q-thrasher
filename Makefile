MACHINE = bruford.cs.unc.edu
CC = gcc
all: thrasher.c
	$(CC) -O2 -o thrasher $^
install: thrasher
	scp thrasher root@$(MACHINE):/tmp/
debug: thrasher.c
	$(CC) -g -o thrasher $^
clean: thrasher.c
	rm thrasher

