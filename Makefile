
all: nanoweb nclient

nanoweb: nanoweb.c
	gcc nanoweb.c -o nanoweb

nclient: client.c
	gcc client.c -o nclient

clean:
	rm nclient nanoweb
