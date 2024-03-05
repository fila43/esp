#include <string.h>

#define NEW_DEVICE 100	// return code for new device 

#define CONNECT	1	// connect to network (new device) initial message
#define TEMP	2	// current temperature
#define STATE	3	// state of barell (ctemperature, dest temperature, onep/closed)
#define CONFIRM	4	// confirm requested change (connect,open/close, COMMAND)
#define COMMAND	5	// OPEN/CLOSE/WATCH/AUTO commands for clients
#define ONLINE	6	// check liveness 

#define SERVER_PORT 8888
#define CLIENT_PORT 8889

/*
 * CONNECT - data -> NULL
 * TEMP - data -> temperature -> int /10
 */


typedef struct message {
	uint8_t type;
	uint8_t id;
	int16_t data;
}Message;

typedef struct device {
	char ip[24];
	unsigned short id;
	char name[64];
}Device;

Message * confirm_queue[100];

int process_message(void * buffer, struct sockaddr_in * net_info, void ** result); 
int new_device(unsigned short id, struct sockaddr_in * net_info, Device ** device); 
int send_ack(Device * d, int16_t seq);
int connect_broadcast(int fd);
int add_to_queue(Message *m);
void print_devices();
