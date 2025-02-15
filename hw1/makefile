all: client server

# Sure, the code could be cleaner, there could have been more
# abstractions and stuff, but I am not overengineering a two week
# assignment just for some brownie points.

client: client.cpp
	g++ client.cpp -o client

server: server.cpp
	g++ server.cpp -o server

run-client: client
	./client 127.0.0.1 25565

run-server: server
	./server 25565
