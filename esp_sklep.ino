#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <sklep_client.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

ESP8266WebServer webServer(80);

const char* ssid = "W_internet";
const char* password = "2122232425";

WiFiUDP udp;
unsigned int localUdpPort = CLIENT_PORT;  // Local port to listen on
unsigned int serverUdpPort = SERVER_PORT;  // Local port to listen on
char incomingPacket[255];  // Buffer for incoming packets
const char* broadcastIP = "255.255.255.255";  // Broadcast IP address
int DEVICE_ID = 1;
unsigned seq = 1;
String server_ip;
float dtemp = 0;
float ctemp = 0;

int state = FSM_OFF;
int state_server = FSM_START;
int device_state = OFF;

#define RELAY 0
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // +1h, update každou minutu

#ifndef AUTO_TEMP
#define AUTO_TEMP 203
#endif
#ifndef AUTO_TIMER
#define AUTO_TIMER 204
#endif
#ifndef FSM_AUTO_TEMP
#define FSM_AUTO_TEMP 53
#endif
#ifndef FSM_AUTO_TIMER
#define FSM_AUTO_TIMER 54
#endif

int auto_mode_type = AUTO_TEMP; // výchozí režim
String device_name = "ESP Testovací modul";

#define MAX_TIMER_INTERVALS 5
struct TimerInterval {
  int on_min;  // minuty od půlnoci
  int off_min;
};
TimerInterval timer_intervals[MAX_TIMER_INTERVALS];
int timer_interval_count = 0;

int waitx(struct waiting * w) {
    w->t++;
    if (w->t%20 == 0)
        return 0;
    else
        return 1;
}

int connect(){

    Message m;
    m.type=CONNECT;
    m.id=DEVICE_ID;
    m.seq=seq++;
    udp.beginPacket(broadcastIP, serverUdpPort);
    udp.write((uint8_t*)&m,sizeof(m));
    udp.endPacket();
    Serial.println("Broadcast message sent.");
    return 0;
}

int connect_to_ip(const char * ipaddr){

    Message m;
    m.type=CONNECT;
    m.id=DEVICE_ID;
    m.seq=seq++;
    udp.beginPacket(ipaddr, serverUdpPort);
    udp.write((uint8_t*)&m,sizeof(m));
    udp.endPacket();
    Serial.println("Connect to server sent.");
    return 0;
}

int send_to_server(Message *m){
    udp.beginPacket(server_ip.c_str(), serverUdpPort);
	
    udp.write((uint8_t*)m,sizeof(Message));
    udp.endPacket();

    return 0;
}

float get_temp(){
    return ctemp;
}

int32_t convert_temp_to_int(float t){
  return (int)(t*100);
}

float convert_temp_int_to_float(int t){
  return (float)(t/100);
}

int send_temp(){
    Message m;
    m.type = TEMP;
    m.id = DEVICE_ID;
    m.data = convert_temp_to_int(get_temp());

    send_to_server(&m);
    return 0;
}

int set_dtemp(int32_t t){
    Serial.print("set dtemp:");
  Serial.print(t);

    dtemp = convert_temp_int_to_float(t);
    return 0;
}

int send_ack(){
	Message m;
	m.type = ONLINE;
	m.id = DEVICE_ID;

	send_to_server(&m);
  return 0;
}

int send_confirm(){
  Message m;
	m.type = CONFIRM;
	m.id = DEVICE_ID;

	send_to_server(&m);
  return 0;
}

int send_state(){
	State s;
  Message *m1;
  State* s2;
	s.type = STATE;
	s.id = DEVICE_ID;
	s.ctemp = convert_temp_to_int(get_temp());
	s.dtemp = convert_temp_to_int(dtemp);
	s.fsm = state;
  Serial.print("state-fsm:");
  Serial.print(state);
  Serial.print("\n");
	s.dstate = device_state;

	m1 = (Message *)&s;
  s2 = (State *)m1;
  Serial.print("state-fsm:");
  Serial.print(s2->fsm);
  Serial.print("\n");
	
  
  send_to_server((Message *) &s);
	return 0;
}

int auto_mode_temp() {
    auto_mode_type = AUTO_TEMP;
    Serial.println("auto_mode_type set to: AUTO_TEMP");
    return 0;
}

int auto_mode_timer() {
    auto_mode_type = AUTO_TIMER;
    Serial.println("auto_mode_type set to: AUTO_TIMER");
    return 0;
}

int on(){
  if (device_state == ON)
    return 0;
    device_state = ON;
    Serial.println("ON");
    digitalWrite(RELAY, LOW);

    return 0;
}

int off(){
  if (device_state == OFF)
    return 0;
  device_state = OFF;
  Serial.println("OFF");
  digitalWrite(RELAY, HIGH);

    return 0;
}

int process_command(Message *m){
    if (m->type != COMMAND)
        return -1;

    switch (m->data) {
        case ON:
            Serial.print("ON\n");
            state = FSM_ON;
            on();
            break;
        case OFF:
            Serial.print("OFF\n");
            state = FSM_OFF;
            off();
            break;
        case AUTO_TEMP:
            Serial.print("AUTO_TEMP\n");
            state = FSM_AUTO_TEMP;
            auto_mode_temp();
            break;
        case AUTO_TIMER:
            Serial.print("AUTO_TIMER\n");
            state = FSM_AUTO_TIMER;
            auto_mode_timer();
            break;
        default:
            ;
    }

    return 0;
}

int process_message(Message *m){
  switch (m->type) {
    case CONFIRM:
    //confirmation of connection request confirm can arrive only after connect request right now
        state_server = FSM_CONNECTED;
        server_ip = udp.remoteIP().toString().c_str();
        return CONNECT;
    case TEMP:
        send_temp();
  Serial.print("TEMP\n");
        break;
    case STATE:
        send_state();
  Serial.print("state \n");
        break;
    case COMMAND:
        if ( process_command(m) == 0 )
            send_confirm();
        break;
    case ONLINE:
        send_ack();
  Serial.print("online\n ");
        break;
    case DTEMP:
    //pass part of message
        set_dtemp(m->data);
        send_confirm();
        break;
    case CONNECT:
        server_ip = udp.remoteIP().toString().c_str();
        state_server = FSM_START;
        Serial.print("New server: ");
        Serial.println(server_ip);
        connect_to_ip(server_ip.c_str());
  }
  return 0;
}

void update_temperature() {
    // TODO update based on sensor
    float a;
    sensors.requestTemperatures();
    ctemp= sensors.getTempCByIndex(0); 
}

void parse_time(const String& t, int& min_out) {
  int h = 0, m = 0;
  sscanf(t.c_str(), "%d:%d", &h, &m);
  min_out = h * 60 + m;
}

// --- Web server s endpointy pro ovládání ---

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
<head>
  <meta charset="UTF-8">
  <title>ESP Ovládání</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; background: #23272e; color: #fff; margin: 0; padding: 0; }
    .container { max-width: 340px; margin: 2.2rem auto 0 auto; padding: 1.1rem 1.2rem 1.1rem 1.2rem; background: #181b20; border-radius: 10px; box-shadow: 0 1px 6px #0005; font-size: 1.05rem; line-height: 1.7; }
    button { font-size: 1rem; padding: 0.4rem 1.1rem; border-radius: 6px; border: none; cursor: pointer; background: #ffb300; color: #23272e; font-weight: bold; margin: 0.3rem 0.5rem; }
    .row { display: flex; gap: 0.5rem; justify-content: center; }
    .input-row { margin: 1.2rem 0 0.5rem 0; }
    .toggle-label { font-weight: bold; cursor: pointer; margin-right: 1.2rem; }
    input[type="radio"] { accent-color: #ffb300; }
    .interval-row { margin-bottom: 0.3rem; }
    .hidden { display: none; }
    .info-box { background: #23272e; border: 1.5px solid #444; border-radius: 8px; padding: 1.1rem 1.2rem; margin-top: 2.2rem; box-shadow: 0 1px 4px #0003; font-size: 1.01rem; }
    .info-box b { color: #ffb300; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP Ovládání</h1>
    <form method="post" action="/set_all">
      <div class="input-row">
        <label for="devname"><b>Název zařízení:</b></label>
        <input type="text" id="devname" name="name" maxlength="32" value="{{DEV_NAME}}">
      </div>
      <div class="input-row">
        <b>Režim:</b><br>
        <label class="toggle-label"><input type="radio" name="mode" id="mode_on" value="on" {{CHECKED_ON}}> ON</label>
        <label class="toggle-label"><input type="radio" name="mode" id="mode_off" value="off" {{CHECKED_OFF}}> OFF</label>
        <label class="toggle-label"><input type="radio" name="mode" id="mode_auto_timer" value="auto_timer" {{CHECKED_TIMER}}> AUTO (časovač)</label>
        <label class="toggle-label"><input type="radio" name="mode" id="mode_auto_temp" value="auto_temp" {{CHECKED_TEMP}}> AUTO (teplota)</label>
      </div>
      <div class="input-row" id="row_temp">
        <label for="targetTemp"><b>Cílová teplota:</b></label>
        <input type="number" id="targetTemp" name="val" min="-50" max="100" step="0.1" value="{{TARGET_TEMP}}"> °C
      </div>
      <div class="input-row" id="row_timer">
        <b>Časovač (intervaly):</b>
        <div class="interval-row">
          <input type="time" name="on1" value="{{ON1}}"> - <input type="time" name="off1" value="{{OFF1}}">
        </div>
        <div class="interval-row">
          <input type="time" name="on2" value="{{ON2}}"> - <input type="time" name="off2" value="{{OFF2}}">
        </div>
        <div class="interval-row">
          <input type="time" name="on3" value="{{ON3}}"> - <input type="time" name="off3" value="{{OFF3}}">
        </div>
        <div class="interval-row">
          <input type="time" name="on4" value="{{ON4}}"> - <input type="time" name="off4" value="{{OFF4}}">
        </div>
        <div class="interval-row">
          <input type="time" name="on5" value="{{ON5}}"> - <input type="time" name="off5" value="{{OFF5}}">
        </div>
      </div>
      <div class="input-row">
        <button type="submit">Uložit vše</button>
      </div>
    </form>
    <div class="info-box">
      <b>Aktuální teplota:</b> <span>{{CUR_TEMP}}</span> °C<br>
      <b>Stav:</b> <span>{{STATE}}</span><br>
      <b>Režim:</b> <span>{{MODE_TXT}}</span><br>
      <b>Název zařízení:</b> <span>{{DEV_NAME}}</span><br>
      <b>IP serveru:</b> <span>{{SERVER_IP}}</span><br>
      <b>Aktuální čas:</b> <span>{{CUR_TIME}}</span>
    </div>
  </div>
  <script>
    function updateFormVisibility() {
      var mode = document.querySelector('input[name="mode"]:checked').value;
      document.getElementById('row_temp').style.display = (mode === 'auto_temp') ? '' : 'none';
      document.getElementById('row_timer').style.display = (mode === 'auto_timer') ? '' : 'none';
    }
    var radios = document.querySelectorAll('input[name="mode"]');
    radios.forEach(function(r) { r.addEventListener('change', updateFormVisibility); });
    window.onload = updateFormVisibility;
  </script>
</body>
</html>
)rawliteral";

String minToTime(int min) {
  if (min < 0 || min >= 24*60) return "";
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", min/60, min%60);
  return String(buf);
}

void handleSetAll() {
  if (webServer.method() == HTTP_POST) {
    // Název
    if (webServer.hasArg("name")) {
      String newName = webServer.arg("name");
      if (newName.length() > 0 && newName.length() < 33) {
        device_name = newName;
      }
    }
    // Teplota
    if (webServer.hasArg("val")) {
      float t = webServer.arg("val").toFloat();
      dtemp = t;
    }
    // Časovač
    timer_interval_count = 0;
    for (int i = 1; i <= MAX_TIMER_INTERVALS; ++i) {
      String onArg = String("on") + String(i);
      String offArg = String("off") + String(i);
      if (webServer.hasArg(onArg) && webServer.hasArg(offArg)) {
        String onVal = webServer.arg(onArg);
        String offVal = webServer.arg(offArg);
        if (onVal.length() == 5 && offVal.length() == 5) {
          parse_time(onVal, timer_intervals[timer_interval_count].on_min);
          parse_time(offVal, timer_intervals[timer_interval_count].off_min);
          timer_interval_count++;
        }
      }
    }
    // Režim
    if (webServer.hasArg("mode")) {
      String m = webServer.arg("mode");
      if (m == "on") {
        state = FSM_ON;
        on();
      } else if (m == "off") {
        state = FSM_OFF;
        off();
      } else if (m == "auto_timer") {
        state = FSM_AUTO_TIMER;
        auto_mode_timer();
      } else if (m == "auto_temp") {
        state = FSM_AUTO_TEMP;
        auto_mode_temp();
      }
    }
  }
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}

void handleRoot() {
  String html = FPSTR(index_html);
  html.replace("{{DEV_NAME}}", device_name);
  html.replace("{{TARGET_TEMP}}", String(dtemp, 1));
  html.replace("{{CUR_TEMP}}", String(ctemp, 1));
  html.replace("{{STATE}}", (device_state == ON) ? "ON" : "OFF");
  String modeStr = (state == FSM_ON) ? "ON" : (state == FSM_OFF) ? "OFF" : (state == FSM_AUTO_TEMP) ? "AUTO (teplota)" : (state == FSM_AUTO_TIMER) ? "AUTO (časovač)" : "?";
  html.replace("{{MODE_TXT}}", modeStr);
  html.replace("{{SERVER_IP}}", server_ip.length() ? server_ip : "-");
  html.replace("{{CHECKED_ON}}", (state == FSM_ON) ? "checked" : "");
  html.replace("{{CHECKED_OFF}}", (state == FSM_OFF) ? "checked" : "");
  html.replace("{{CHECKED_TIMER}}", (state == FSM_AUTO_TIMER) ? "checked" : "");
  html.replace("{{CHECKED_TEMP}}", (state == FSM_AUTO_TEMP) ? "checked" : "");
  html.replace("{{CUR_TIME}}", timeClient.getFormattedTime());
  for (int i = 0; i < MAX_TIMER_INTERVALS; ++i) {
    String onKey = String("{{ON") + String(i+1) + "}}";
    String offKey = String("{{OFF") + String(i+1) + "}}";
    html.replace(onKey, (i < timer_interval_count) ? minToTime(timer_intervals[i].on_min) : "");
    html.replace(offKey, (i < timer_interval_count) ? minToTime(timer_intervals[i].off_min) : "");
  }
  webServer.send(200, "text/html; charset=utf-8", html);
}

void handleOn() {
  state = FSM_ON;
  on();
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}
void handleOff() {
  state = FSM_OFF;
  off();
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}
void handleSetMode() {
  if (webServer.method() == HTTP_POST && webServer.hasArg("mode")) {
    String m = webServer.arg("mode");
    if (m == "temp") {
      state = FSM_AUTO_TEMP;
      auto_mode_temp();
    } else if (m == "timer") {
      state = FSM_AUTO_TIMER;
      auto_mode_timer();
    }
  }
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}
void handleSetTemp() {
  if (webServer.method() == HTTP_POST && webServer.hasArg("val")) {
    float t = webServer.arg("val").toFloat();
    dtemp = t;
  }
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}
void handleSetName() {
  if (webServer.method() == HTTP_POST && webServer.hasArg("name")) {
    String newName = webServer.arg("name");
    if (newName.length() > 0 && newName.length() < 33) {
      device_name = newName;
    }
  }
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}
void handleSetTimer() {
  if (webServer.method() == HTTP_POST) {
    timer_interval_count = 0;
    for (int i = 1; i <= MAX_TIMER_INTERVALS; ++i) {
      String onArg = String("on") + String(i);
      String offArg = String("off") + String(i);
      if (webServer.hasArg(onArg) && webServer.hasArg(offArg)) {
        String onVal = webServer.arg(onArg);
        String offVal = webServer.arg(offArg);
        if (onVal.length() == 5 && offVal.length() == 5) {
          parse_time(onVal, timer_intervals[timer_interval_count].on_min);
          parse_time(offVal, timer_intervals[timer_interval_count].off_min);
          timer_interval_count++;
        }
      }
    }
  }
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}
void handleSetPower() {
  if (webServer.method() == HTTP_POST && webServer.hasArg("power")) {
    String p = webServer.arg("power");
    if (p == "on") {
      state = FSM_ON;
      on();
    } else if (p == "off") {
      state = FSM_OFF;
      off();
    }
  }
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}

void run_fsm(){
    update_temperature();
    float h = 0.3;
    switch (state_server) {
        case FSM_START: {
            static Wait w;
            w.waitx = &waitx;
            if (w.waitx(&w) == 0){
                Serial.print("T- ");
                Serial.print(w.t);
                Serial.println();
                connect();
            }
            break;
        }
    }
    switch (state) {
        case FSM_AUTO_TEMP: {
            float x = dtemp - ctemp;
            if (x < 0 - h)
                on();
            if (x>=0)
                off();
            break;
        }
        case FSM_AUTO_TIMER: {
            int now_min = timeClient.getHours() * 60 + timeClient.getMinutes();
            bool active = false;
            for (int i = 0; i < timer_interval_count; ++i) {
                int on_min = timer_intervals[i].on_min;
                int off_min = timer_intervals[i].off_min;
                if (on_min <= off_min) {
                    if (now_min >= on_min && now_min < off_min) active = true;
                } else {
                    if (now_min >= on_min || now_min < off_min) active = true;
                }
            }
            if (active) on(); else off();
            break;
        }
        case FSM_ON:
        case FSM_OFF:
            // no action should be needed
            break;
    }
}

void setup() {
  Serial.begin(115200);
  delay(10);
  pinMode(RELAY, OUTPUT); 
    digitalWrite(RELAY, HIGH);
  




  sensors.begin();

  // Connect to Wi-Fi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start UDP
  udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

  connect();
  // --- NTP ---
  timeClient.begin();
  timeClient.update();
  Serial.print("NTP time: ");
  Serial.println(timeClient.getFormattedTime());

  SPIFFS.begin();
  webServer.on("/", handleRoot);
  webServer.on("/set_all", HTTP_POST, handleSetAll);
  webServer.on("/on", HTTP_POST, handleOn);
  webServer.on("/off", HTTP_POST, handleOff);
  webServer.on("/set_mode", HTTP_POST, handleSetMode);
  webServer.on("/set_temp", HTTP_POST, handleSetTemp);
  webServer.on("/set_name", HTTP_POST, handleSetName);
  webServer.on("/set_timer", HTTP_POST, handleSetTimer);
  webServer.on("/set_power", HTTP_POST, handleSetPower);
  webServer.begin();
}

void loop() {
    timeClient.update();
    webServer.handleClient();

    int packetSize = udp.parsePacket();
    char incomingPacket[255];
    if (packetSize) {
        // Receive incoming UDP packets
        int len = udp.read(incomingPacket, 255);
        if (len > 0) {
            incomingPacket[len] = 0;
        }

        Message m;
        memcpy(&m, incomingPacket, sizeof(m));
        switch (process_message(&m)) {
            case CONNECT:
                Serial.print("CONNECTED\n");
                break;
            case ON:
                break;
            case OFF:
                break;
            case AUTO_TEMP:
            case AUTO_TIMER:
                break;
            default:
                break;
        }
    }
  /*  if(Serial.available()>0)
  {
     int incomingByte = Serial.parseInt();
     ctemp = (float)incomingByte/100.0;
    Serial.println(ctemp);
    //Serial.print(incomingByte);
  }*/
    run_fsm();
    delay(1000);
}
