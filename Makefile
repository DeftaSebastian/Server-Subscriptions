build: server subscriber

server:
	gcc server.c -o server -lm

subscriber:
	gcc subscriber.c -o subscriber

.PHONY: clean

clean:
	rm -rf server subscriber *.o
