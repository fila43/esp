#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "communication.h"

#define PORT     8888
#define MAXLINE 1024

int sockfd;
Device * devices[21];

int main (int argc, char* argv[]) {
	struct sockaddr_in servaddr, cliaddr;
	char buffer[MAXLINE];
	void * result;

	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        	printf("socket creation failed");
        	exit(1);
	}

	// Filling server information
    	servaddr.sin_family    = AF_INET; // IPv4
    	servaddr.sin_addr.s_addr = INADDR_ANY;
    	servaddr.sin_port = htons(PORT);

	// Bind the socket with the server address
    	if ( bind(sockfd, (const struct sockaddr *)&servaddr,
            sizeof(servaddr)) < 0 ) {
        	printf("bind failed");
        	exit(1);
    	}

	int n;
	socklen_t len;
	char str[100];
	Message  m;

	len = sizeof(cliaddr);
	while (1) {
		n = recvfrom(sockfd, &m, sizeof(Message),
                	MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                	&len);

		if (n > 1) {
			n = process_message(&m, &cliaddr, &result);
			switch (n) {
				case NEW_DEVICE:
					Device *d = (Device *) result;
					if (d->id > 20){
						printf("device id > 20\n");
						return -1;
					}
					devices[d->id] = d;
					print_devices();
				default:
						;
			}
		}
	}
	//printf("connected - %d-%s\n",((Device*)result)->id,((Device*)result)->ip);

/*
	buffer[n] = '\0';
	printf("%s\n",buffer);
	inet_ntop(AF_INET, &(cliaddr.sin_addr), str, INET_ADDRSTRLEN);
	printf("%s:", str);
	printf("%d\n",ntohs(cliaddr.sin_port));
*/
	return 0;
}

void print_devices(){
	for (int i=0; i< 21; i++){
		if (devices[i] == NULL)
			continue;
		printf("%s - %d - %s\n",devices[i]->name, devices[i]->id, devices[i]->ip);
	}
	return;
}

int process_message (void * buffer, struct sockaddr_in * net_info, void ** result){
	Message * m;
	int res;
	m = (Message *) buffer;

	switch (m->type) {
		case CONNECT:
			res = new_device(m->id, net_info, (Device **) result);
			if ( res == 0)
				 if (send_ack((Device*)(*result), m->data) != 0)
					 return -1;
			return NEW_DEVICE;
		default:
			printf("other");

		}
}

int new_device (unsigned short id, struct sockaddr_in * net_info, Device ** device){
	*device = (Device*)malloc(sizeof(Device));
	if ((*device) == NULL)
		return -1;

	(*device)->id = id;
	memset((*device)->ip, '\0', 24);
	inet_ntop(AF_INET,&(net_info->sin_addr), (*device)->ip, INET_ADDRSTRLEN);

	return 0;
}

int send_ack(Device * d, int16_t seq){
	struct sockaddr_in dest;
	Message m;
	int n;

	dest.sin_family    = AF_INET; // IPv4
 	dest.sin_addr.s_addr = inet_addr(d->ip);
	dest.sin_port = htons(CLIENT_PORT);

	m.type = CONFIRM;
	m.id = d->id;
	m.data = seq;

	n = sendto(sockfd, &m, sizeof(m), 0, ( struct sockaddr *) &dest, sizeof(dest));
	if (n < 0){
		printf("chyba: %s\n",strerror(errno));
		return -1;
	}

	return 0;
}
