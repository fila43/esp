#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "communication.h"

#define DEVICE_ID     1
#define MAXLINE 1024

uint8_t seq = 0;

int main (int argc, char* argv[]) {
	int sockfd;
	struct sockaddr_in servaddr, cliaddr;
	char buffer[MAXLINE];
	int len = sizeof(cliaddr);

	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        	printf("socket creation failed");
        	exit(1);
	}
	//set timeout for socket for initial connection
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    		printf("Error in timming");
	}

        int broadcastEnable=1;
        int ret=setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
	if (ret < 0){
		printf("cant set broadcast\n");
		return ret;
	}
	// Filling server information
    	servaddr.sin_family    = AF_INET; // IPv4
    	servaddr.sin_addr.s_addr = INADDR_ANY;
    	servaddr.sin_port = htons(CLIENT_PORT);

	// Bind the socket with the server address
    	if ( bind(sockfd, (const struct sockaddr *)&servaddr,
            sizeof(servaddr)) < 0 ) {
        	printf("bind failed");
        	exit(1);
    	}

	// Initiate connection and wait for confirm from server
	// if there is no response broadcast is repeated
	while (1) {
		if (connect_broadcast(sockfd) < 0){
			printf("chyba: %s",strerror(errno));
			return -1;
		}
		Message m;
		int n = recvfrom(sockfd, &m, sizeof(Message), MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);
		if (n > 0 && m.type == CONFIRM && m.data == ( seq - 1 ) ){
			printf("Connected\n");
			break;
		}
		printf("new attempt to connect\n");
	}
	while (1){
		Message m;
		int n = recvfrom(sockfd, &m, sizeof(Message), MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len);

		switch (m.type) {
			;
		}
	}

	return 0;
}

int connect_broadcast( int fd){
	struct sockaddr_in cliaddr;
	Message  m;
	int n;
        socklen_t len;

	cliaddr.sin_family    = AF_INET; // IPv4
        cliaddr.sin_addr.s_addr = INADDR_BROADCAST;
        cliaddr.sin_port = htons(SERVER_PORT);



        len = sizeof(cliaddr);

        m.type = CONNECT;
        m.id = DEVICE_ID;
	m.data = seq++;
        n = sendto(fd, &m, sizeof(m), 0, ( struct sockaddr *) &cliaddr, len);

	return n;

}

int add_to_queue( Message * m){
	for (int i=0; i<100; i++)
		if (confirm_queue[i] != NULL){
			confirm_queue[i] = m;
			return 0;
		}
	return -1;	//queue is full
}


