#include <string.h>
#include <unistd.h>
#define NEW_DEVICE 100	// return code for new device 

// message types
#define CONNECT	1	// connect to network (new device) initial message
#define TEMP	2	// current temperature
#define STATE	3	// state of barell (ctemperature, dest temperature, onep/closed)
#define CONFIRM	4	// confirm requested change (connect,open/close, COMMAND)
#define COMMAND	5	// OPEN/CLOSE/WATCH/AUTO commands for clients
#define ONLINE	6	// check liveness
#define DTEMP	7	// set required temperature server -> client
#define POWER	8	// power measurement

// define commands
#define ON 201
#define OFF 202
#define AUTO_TEMP 203
#define AUTO_TIMER 204

#define SERVER_PORT 8888
#define CLIENT_PORT 8889

#define FSM_START 49
#define FSM_CONNECTED 50
#define FSM_ON 51
#define FSM_OFF 52
#define FSM_AUTO 53

#define DEV_TYPE_POWER_METER 1
#define DEV_TYPE_ESP01 2
/*
 * CONNECT - data -> NULL
 * TEMP - data -> temperature -> int /10
 */


typedef struct message {
	uint8_t type;
	uint32_t id;        // Changed from uint8_t to uint32_t for ESP Chip ID
	uint8_t seq;
	int32_t data;
    uint32_t padding;
    uint8_t padding1;
}Message;

typedef struct state {
	uint8_t type;
	uint32_t id;        // Changed from uint8_t to uint32_t for ESP Chip ID
	int32_t ctemp;
	int32_t dtemp;
	uint8_t fsm;
	uint8_t dstate;		// device state - ON/OFF/AUTO - use commands definitions
}State;

//typedef struct device {
//	char ip[24];
//	unsigned short id;
//	char name[64];
//}Device;

extern Message * confirm_queue[100];

int process_message(void * buffer, struct sockaddr_in * net_info, void ** result); 
int connect_broadcast(int fd);
int add_to_queue(Message *m);
void print_devices();
int fsm_start(int fd);
int on();
int off();
int auto_mode();
int send_state();
int send_client_ack();
int send_temp();
uint8_t get_temp();		// returns converted temp to int
void send_to_server(Message * data);
void set_dtemp(uint8_t temp);
