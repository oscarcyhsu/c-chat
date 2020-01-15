all:client.cpp server.cpp
	g++ -o client client.cpp -lpthread
	g++ server.cpp -lpthread -o server
clean:
	rm -f client
	rm -f server

