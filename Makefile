
all: nweb23 nclient

nweb23: nweb23.c
	gcc nweb23.c -o nweb23

nclient: client.c
	gcc client.c -o nclient

clean:
	rm nclient nweb23
