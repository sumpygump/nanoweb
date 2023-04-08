all: nanoweb nclient

nanoweb: nanoweb.c
	gcc nanoweb.c -o nanoweb

nclient: client.c
	gcc client.c -o nclient

debug: nanoweb.c
	gcc -Wall -g nanoweb.c -o nanoweb

clean:
	rm nclient nanoweb

lint:
	astyle --suffix=none --options=.astylerc 'nanoweb.c' 'client.c'
