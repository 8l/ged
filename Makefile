CFLAGS+=-O3
ged: ged.o
	cc ${CFLAGS} -o ged ged.o `pkg-config --libs gtk+-2.0`
ged.o: ged.c
	cc ${CFLAGS} -c ged.c `pkg-config --cflags gtk+-2.0`
clean:
	rm -f ged.o ged *~