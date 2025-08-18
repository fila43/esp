#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctime>
#include <chrono>
#include <mutex>

#include "server.h"

#define PORT     8888
#define MAXLINE 1024
#define SERVER_ID 0

int sockfd;
Device * devices[21];
uint8_t seq = 0;

int main (int argc, char* argv[]) {
    Api a;
    Cli i = Cli(&a);

    // Spuštění unix socket serveru
    UnixSocketServer unixServer(&a);
    unixServer.start();

    // Spuštění HTTP serveru
    HttpServer httpServer(&a, 8080);
    httpServer.start();

    Devices d;
    Connection c = Connection(PORT,&a);
    a.c = c;
    if (c.broadcast_hello_message())
        std::cout<<"Hello message sent"<<std::endl;
    else
        std::cout<<"Error occured during broadcasting hello message"<<std::endl;

    c.bindPort();
    std::thread t = c.start_listen();

    int x;
    int d_id;
    int d_id1;
    i.start();
    return 1;
    Watcher w;
    std::thread t1;
	scanf("%d",&d_id1);
    while(1){
	a.devs.print_devices();
	std::cout<<"devices\n";
	scanf("%d",&d_id);
	std::cout<<"1-on\n2-off\n3-state\n4-set_auto\n5-ctemp\n6-watcher\n";
	scanf("%d",&x);
	Device * dev = a.devs.get_device(d_id);
	switch (x){
		case 1:
			dev->set_on();
			break;
		case 2:
			dev->set_off();
			break;
		case 3:
			dev->request_state();
			break;
		case 4:
			dev->set_auto_mode();
			break;
		case 5: dev->request_ctemp();
			break;
        case 6:
            w.add_api(&a);
            w.set_delay(20);
            t1 = w.start_watch();
            break;
		default:
			;
    }
    }
    t1.join();
	t.join();

	/*
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
    	Device * d;
	*/
/*
	len = sizeof(cliaddr);
	while (1) {
		n = recvfrom(sockfd, &m, sizeof(Message),
                	MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                	&len);

		if (n > 1) {
			n = process_message(&m, &cliaddr, &result);
			switch (n) {
				case NEW_DEVICE:
					d = (Device *) result;
					// TODO check device id and range of array
					devices[d->get_id()] = d;
					 //print_devices();
					break;
				default:
						;
			}
			
			int x;
			std::cout<<"1-request_ctemp\n2-request_state\n3-set_on\n \
				5-set_auto\n6-set_temp\n";
			scanf("%d",&x);
			printf("%d\n",x);
			Device *d = devices[0];
			switch (x){
				case 1:
					d->request_ctemp();
					break;
				case 2:
					d->request_state();
					break;
				case 3:
					d->set_on();
					break;
				case 4:
					d->set_off();
					break;
				case 5:
					d->set_auto_mode();
					break;
				case 6:
					d->set_temp(20);
					break;
				default:
					continue;

			}
		}

	}
	*/
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
Connection::Connection(){}
Connection::Connection(in_port_t port=PORT, Api * a){
	if ( (this->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        	printf("socket creation failed");
        	exit(1);
	}
    	this->serv.sin_family    = AF_INET; // IPv4
    	this->serv.sin_addr.s_addr = INADDR_ANY;
    	this->serv.sin_port = htons(port);

	this->api = a;
}
void Connection::bindPort(){
	// Bind the socket with the server address
    	if ( bind(this->fd, (const struct sockaddr *)&serv,
            sizeof(serv)) < 0 ) {
        	printf("bind failed");
        	exit(1);
    	}
}
bool Connection::broadcast_hello_message(){
    int bfd;
    int broadcastPermission = 1;
    struct sockaddr_in broadcastAddr;
    std::string bip = "255.255.255.255";

    if ((bfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return false;

    if (setsockopt(bfd, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission,
                   sizeof(broadcastPermission)) < 0)
        return false;

    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = inet_addr(bip.data());
    broadcastAddr.sin_port = htons(CLIENT_PORT);

    int messageLen = sizeof(Message);
    Message m;

    m.type = CONNECT;
    m.id = 0;
    m.seq = 0;
    //data should be empty so let's fill it with the server port
    m.data = SERVER_PORT;

    if (sendto(bfd, &m, messageLen, 0, (struct sockaddr *) &broadcastAddr,
               sizeof(broadcastAddr)) != messageLen)
        return false;

    close(bfd);
    return true;
}
void Connection::listen(){
	Message *m = new Message;
	struct sockaddr_in sender;
	//need to be initialized
	//https://stackoverflow.com/questions/18915676/inet-ntop-returns-0-0-0-0-in-first-usage
	socklen_t len = sizeof(struct sockaddr);
	while(1){
		int n = recvfrom(fd, m, sizeof(Message),
			MSG_WAITALL, ( struct sockaddr *) &sender,&len);

		char ip[30];
		memset(ip,'\0',24);
		inet_ntop(AF_INET,&(sender.sin_addr), ip, sizeof(ip));
		std::cout<<"ip:"<<ip<<" ID: "<<(int)m->id<<std::flush<<std::endl;

        Device * d = api->devs.get_device(m->id);
		if (d == NULL ){
            std::cout<<"device not found"<<std::endl;
             d = api->new_device(std::string(ip),m->id);
        }

		api->process_message(d,m);
	}

}
std::thread Connection::start_listen(){
	return std::thread(&Connection::listen,this);
}
Device::Device(std::string ip, uint8_t id){
	this->ip = ip;
	this->id = id;
}
Device::Device(std::string ip, std::string name, uint8_t id){
	this->ip = ip;
	this->name = name;
	this->id = id;
}

uint8_t Device::get_id(){
	return this->id;
}

int Device::request_ctemp(){
	Message m;
	m.id = SERVER_ID;
	m.type = TEMP;
	m.seq = seq++;

	this->send_message(&m);

	return 0;
}

int Device::request_state() {
	Message m;
	m.id = SERVER_ID;
	m.type = STATE;
	m.seq = seq++;

	this->send_message(&m);

	return 0;
}

int Device::set_on(){
	Message m;
	m.id = SERVER_ID;
	m.type = COMMAND;
	m.seq = seq++;
	m.data = ON;

	this->send_message(&m);

	return 0;
}

int Device::set_off(){
	Message m;
	m.id = SERVER_ID;
	m.type = COMMAND;
	m.seq = seq++;
	m.data = OFF;

	this->send_message(&m);

	return 0;
}
int Device::set_auto_mode(int mode_type){
	Message m;
	m.id = SERVER_ID;
	m.type = COMMAND;
	m.seq = seq++;
	m.data = mode_type;

	this->send_message(&m);

	return 0;
}
int Device::set_temp(float temp){
	Message m;
	m.id = SERVER_ID;
	m.type = DTEMP;
	m.seq = seq++;
	m.data = (int)(temp*100);

	this->send_message(&m);

	return 0;
}

Device * Devices::get_device(uint8_t id){
    std::cout<<"argument ID: "<<(unsigned int)id<<std::endl;
	for (auto d: this->devices)
		if (d->id == id){
            d->print();
			return d;
        }
	return NULL;
}
//std::string Device::toString(){
//	return std::string(this->id)+" "+this->name+" "+this->ip+"\n";
//}
/*
void print_devices(){
	for (int i=0; i< 21; i++){
		if (devices[i] == NULL)
			continue;
		printf("%s - %d - %s\n",devices[i]->name, devices[i]->id, devices[i]->ip);
	}
	return;
}
*/
int Api::process_message (Device * d, Message *m){
	int res;
    std::cout<<"new message to process"<<std::endl;
    std::cout<<(unsigned int)m->type<<std::endl;

	switch (m->type) {
		case CONNECT:
			if (this->devs.get_device(d->id) != NULL){
				//already known device
				//lets send him just ack
				d->send_ack(m->seq);
				break;
			}
			if (d->send_ack(m->seq) != 0)
				return -1;
			this->devs.add_device(d);
			printf("connect\n");
			return NEW_DEVICE;
		case STATE:
			printf("state\n");
            if (!d->parse_state((State *) m))
                std::cout<<"error in parsing state - dtemps differs";
            d->print_state();
			break;
		case TEMP:
			d->ctemp = m->data;
			printf("ctemp: %d\n",m->data);
			break;
		case CONFIRM:
			printf("confirm\n",m->data);
			break;
		case ONLINE:
			printf("online\n",m->data);
			break;

		default:
			printf("data: %u\n",m->data);


		}
	return 0;
}
bool Device::parse_state(State * s){
    this->ctemp = s->ctemp;
    //if (this->dtemp != s->dtemp)
    //    return false;
    this->dtemp = s->dtemp;
    this->fsm = s->fsm;
    this->dstate = s->dstate;
    return true;
}
Device * Api::new_device(std::string ip, uint8_t id){
	Device *d = new Device(ip, id);
	d->api = this;
	return d;
}
int new_device (unsigned short id, struct sockaddr_in * net_info, Device ** device){
	char ip[30];
	if ((*device) == NULL)
		return -1;

	(*device)->id = id;
	memset(ip, '\0', 24);
	inet_ntop(AF_INET,&(net_info->sin_addr), ip, INET_ADDRSTRLEN);

	*device = new Device(std::string(ip),"",id);

	return 0;
}
int Device::send_message(Message * m){
	struct sockaddr_in dest;
	int n;
	std::cout<<"SENDING TO->DEST IP: "<<this->ip<<std::endl;
	dest.sin_family    = AF_INET; // IPv4
 	dest.sin_addr.s_addr = inet_addr(this->ip.data());
	dest.sin_port = htons(CLIENT_PORT);

	n = sendto(this->api->c.fd, m, sizeof(Message), 0, ( struct sockaddr *) &dest, sizeof(dest));
	if (n < 0){
		printf("chyba: %s\n",strerror(errno));
		return -1;
	}
	return 0;

}
void Device::print(){
	std::cout<<"ID: "<<(int)this->id<<" IP:"<<this->ip<<" NAME:"<<this->name<<std::endl;
}
void Device::print_state(){
    std::cout<<"|----------------------------------\n| ";
    this->print();
    std::cout<<"| Current temp: "<<(int)this->ctemp<<std::endl;
    std::cout<<"| Destination temp: "<<(int)this->dtemp<<std::endl;
    std::cout<<"| FSM state: "<<code_to_str(this->fsm)<<std::endl;
    std::cout<<"| Device state: "<<code_to_str(this->dstate)<<std::endl;
    std::cout<<"|----------------------------------"<<std::endl;
}
void Devices::print_devices(){
	for (auto d:this->devices)
		d->print();
}
int Device::send_ack(uint16_t seq){
	Message m;

	m.type = CONFIRM;
	m.id = this->id;
	m.seq = seq;

	return this->send_message(&m);

}
bool Devices::add_device(Device *d){
	devices.emplace_back(d);
	return true;
}

void Watcher::add_api(Api * ap){
    this->a = ap;
    //MOVE to constructor
    this->delay = 10;
}
void Watcher::set_delay(unsigned d){
    this->delay = d;
}
std::thread Watcher::start_watch(){
    return std::thread(&Watcher::watch,this);
}
void Watcher::watch(){
    while(1){
        for (auto d: this->a->devs.devices){
            d->request_ctemp();
        }
    sleep(this->delay);
    if (this->stop){
        this->stop = false;
        return;
        }
    }
}
std::thread Watcher::start_periodic_state_query(unsigned period_sec) {
    return std::thread([this, period_sec]() {
        while (true) {
            for (auto d : this->a->devs.devices) {
                d->request_state();
            }
            sleep(period_sec);
            if (this->stop) {
                this->stop = false;
                return;
            }
        }
    });
}
// Pomocná funkce: převod HH:MM na minuty od půlnoci
static int time_to_minutes(const std::string& t) {
    int h, m;
    if (sscanf(t.c_str(), "%d:%d", &h, &m) == 2) {
        return h * 60 + m;
    }
    return 0;
}

void Watcher::set_schedule(int device_id, const std::vector<std::pair<std::string, std::string>>& schedule) {
    // Pokud už běží thread pro device_id, zastav ho
    stop_schedule(device_id);
    stop_flags[device_id] = false;
    schedule_threads[device_id] = std::thread([this, device_id, schedule]() {
        bool last_on = false;
        while (true) {
            if (stop_flags[device_id]) return;
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            struct tm* parts = std::localtime(&now_c);
            int current_minutes = parts->tm_hour * 60 + parts->tm_min;
            bool should_be_on = false;
            for (const auto& p : schedule) {
                int on_min = time_to_minutes(p.first);
                int off_min = time_to_minutes(p.second);
                if (on_min <= off_min) {
                    if (current_minutes >= on_min && current_minutes < off_min)
                        should_be_on = true;
                } else {
                    if (current_minutes >= on_min || current_minutes < off_min)
                        should_be_on = true;
                }
            }
            Device* dev = this->a->devs.get_device(device_id);
            if (dev) {
                if (should_be_on && !last_on) {
                    dev->set_on();
                    last_on = true;
                } else if (!should_be_on && last_on) {
                    dev->set_off();
                    last_on = false;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    });
}

void Watcher::stop_schedule(int device_id) {
    if (schedule_threads.count(device_id)) {
        stop_flags[device_id] = true;
        if (schedule_threads[device_id].joinable())
            schedule_threads[device_id].join();
        schedule_threads.erase(device_id);
        stop_flags.erase(device_id);
    }
}
void Cli::print_devices(){
    this->print_header();
    this->print_table();
}
void Cli::print_header(){
    std::cout << std::string(74,'-') << std::endl;
    std::cout<< "|"<<std::setw(47) << std::right<<"Connected devices"<<std::setw(26)
        <<"|"<<std::endl;
    std::cout << std::string(74,'-') << std::endl;
    std::cout << "| ID |" << std::setw(9)<<std::right<<"IP"<<std::setw(9)<<" | "
        << std::setw(8)<<std::right<<"Name"<<std::setw(8)<<" | "
        << std::setw(1)<<std::right<<"Current Temp"<<std::setw(1)<<" | "
        << std::setw(1)<<std::right<<"Dest temp"<<std::setw(1)<<" | "
        << std::setw(1)<<std::right<<"State"<<std::setw(1)<<" |"<<std::endl;
    std::cout << std::string(74,'-') << std::endl;

}
void Cli::print_table(){
	for ( auto d: this->a->devs.devices){
    std::cout << "| "<<(int)d->id<<" |" << std::setw(16)<<std::right<<d->ip<<" | "
        << std::setw(13)<<std::right<<d->name<<" | "
        << std::setw(12)<<std::right<<(int)d->ctemp<<" | "
        << std::setw(9)<<std::right<<(int)d->dtemp<<" | "
        << std::setw(5)<<std::right<<code_to_str(d->dstate)<<" |"<<std::endl;
    std::cout << std::string(74,'-') << std::endl;
	}
}
void  Cli::print_commands(){

    std::cout << std::string(51,'-') << std::endl;

    std::cout <<"|" <<std::string(16,' ') << "COMMANDS" << std::string(26, ' ')<<"|"<< std::endl;
    std::cout << std::string(51,'-') << std::endl;
    std::cout << "| ON | OFF | STATE | AUTO | TEMP | DTEMP | REFRESH"<<" |" << std::endl;
    std::cout << std::string(51,'-') << std::endl;
}
int Cli::select_device(){
	int d;
	std::cout<<"Select device: ";
	std::cin>>d;
	return d;
}
std::string Cli::select_command(){
	std::string c;
	while(1){
		std::cout<< "Select command: "<<std::flush;
		std::cin >> c;
		if (c == "ON" || c == "OFF" || c == "STATE" || c == "AUTO"
				|| c == "TEMP" || c == "DTEMP" || c == "on"
				|| c == "off" || c == "auto" || c == "temp"
				|| c == "dtemp" || c =="r" || c == "w"
				|| c == "REFRESH" || c == "s" || c == "state" )
			//r-refresh\ w-watch temps
			return c;
	}
	return "";
}


void Cli::start(){
	int n;
	std::thread t1;
    	std::cout<<"Server is running on port "<<PORT<<std::endl;
	std::cout<<"Connecting";
	for (int i=0; i<10; i++){
    		usleep(500000); //wait for device connecting
		std::cout<<"."<<std::flush;
	}
	std::cout<<std::endl;
start:	this->print_devices();
	this->print_commands();
	std::string c = this->select_command();
	if (c == "r")
		goto start;
	if (c == "w"){
		std::cout<<"starting watcher"<<std::endl;
		Watcher w;
            w.add_api(this->a);
            w.set_delay(20);
            t1 = w.start_watch();
		goto start; //TODO start watching , but first check if already started watcher
	    }
dvc:	int d = this->select_device();

	Device *dev = this->a->devs.get_device(d);
	if (dev == NULL){
		std::cout<<"unknown device try it again"<<std::endl;
		goto dvc;
	}
	this->run_command(dev, c);
//	    t1.join();
	goto start;


}
bool Cli::run_command(Device *d, std::string c){
	if (c == "ON" || c == "on" )
		d->set_on();
	if (c == "OFF" || c == "off" )
		d->set_off();
	if (c == "AUTO" || c == "auto" )
		d->set_auto_mode();
	if (c == "STATE" || c == "state" || c =="s")
		d->request_state();
	if (c == "TEMP" || c == "temp" )
		d->request_ctemp();
	if (c == "DTEMP" || c == "dtemp" ){
		float t;
		std::cout<<"Set temperature: ";
		std::cin>>t;
		d->set_temp(t);
	}

	return true;
}
Cli::Cli(Api *a){
	    this->a = a;
    }
std::string code_to_str(uint8_t c){
	switch (c){
		case 201:
			return "ON";
		case 202:
			return "OFF";
		case 203:
			return "AUTO";
		case 49:
			return "FSM_START";
		case 50:
			return "FSM_CONNECTED";
		case 51:
			return "FSM_ON";
		case 52:
			return "FSM_OFF";
		case 53:
			return "FSM_AUTO";
	}
	return "";
}

// Implementace konstruktoru Watcher
Watcher::Watcher() : a(nullptr), delay(0), stop(false) {}

// Implementace singletonu
Watcher& Watcher::instance() {
    static Watcher inst;
    return inst;
}














