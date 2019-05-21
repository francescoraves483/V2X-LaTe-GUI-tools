CC = gcc
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -lslope
CFLAGS = -I. -O2 $(shell pkg-config --cflags gtk+-3.0) -lslope
GTKBIN = iperfLauncher
GTKSRC = iperfLauncher.c

all:
	$(CC) $(CFLAGS) -o $(GTKBIN) $(GTKSRC) $(LDFLAGS)

make_and_run:
	$(CC) $(CFLAGS) -o $(GTKBIN) $(GTKSRC) $(LDFLAGS)
	./iperfLauncher

make_and_run_debug:
	$(CC) -g $(CFLAGS) -o $(GTKBIN) $(GTKSRC) $(LDFLAGS)
	./iperfLauncher

clean:
	rm $(GTKBIN)
