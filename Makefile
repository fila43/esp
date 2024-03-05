all:server.c client.c
	gcc  server.c -g -o server
	gcc  client.c -g -o client
