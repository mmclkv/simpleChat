all: server client

clean:
	rm -f server client

server: server.cc
	g++ -std=c++11 -o $@ $^

client: client.cc
	g++ -std=c++11 -o $@ $^ -lpthread

.PHONY: all clean
