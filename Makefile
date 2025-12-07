all:server.c client.c unix_socket_server.cpp http_server.cpp db_api.cpp collector.cpp
	g++-15 server.c unix_socket_server.cpp http_server.cpp db_api.cpp collector.cpp -g -o server \
		-I/opt/homebrew/opt/libpq/include -L/opt/homebrew/opt/libpq/lib -lpq \
		-I/opt/homebrew/opt/curl/include -L/opt/homebrew/opt/curl/lib -lcurl \
		-I/opt/homebrew/opt/nlohmann-json/include \
		-std=c++17
#	g++-14  server.c -g -o server
	#gcc  client.c -g -o client
