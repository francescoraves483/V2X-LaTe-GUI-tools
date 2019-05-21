CC = gcc
LDFLAGS = $(shell pkg-config --libs gtk+-3.0)
CFLAGS = -I. -O2 $(shell pkg-config --cflags gtk+-3.0)
GTKBIN = APUbitrateset
GTKSRC = APUbitrateset.c

all:
	$(CC) $(CFLAGS) -o $(GTKBIN) $(GTKSRC) $(LDFLAGS)

make_and_run:
	$(CC) $(CFLAGS) -o $(GTKBIN) $(GTKSRC) $(LDFLAGS)
	./APUbitrateset

make_and_run_debug:
	$(CC) -g $(CFLAGS) -o $(GTKBIN) $(GTKSRC) $(LDFLAGS)
	./APUbitrateset

clean:
	rm $(GTKBIN)