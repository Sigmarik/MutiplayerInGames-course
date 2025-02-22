all: client server

# Sure, the code could be cleaner, there could have been more
# abstractions and stuff, but I am not overengineering a two week
# assignment just for some brownie points.

client: client.cpp connections.cpp
	g++ -g client.cpp connections.cpp -o client

server: server.cpp connections.cpp
	g++ -g server.cpp connections.cpp -o server

run-client: client
	./client 127.0.0.1 25565

run-server: server
	./server 25565
