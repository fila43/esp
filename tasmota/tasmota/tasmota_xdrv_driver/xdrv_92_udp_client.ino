/*
  xdrv_92_udp_client.ino - UDP client driver for Tasmota

  Copyright (C) 2024  Your Name

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_UDP_CLIENT

#define XDRV_92           92

/*********************************************************************************************\
 * Constants
\*********************************************************************************************/

#define UDP_CLIENT_PORT    8889                    // UDP client port (local)
#define UDP_SERVER_PORT    8888                    // UDP server port (remote)
#define UDP_BUFFER_SIZE    255                     // UDP buffer size
#define UDP_BROADCAST_IP   "255.255.255.255"      // Broadcast IP address

// Message types
#define CONNECT            1
#define CONFIRM            4
#define POWER_MEASUREMENT              8

// Device types  
#define DEV_TYPE_POWER_METER 1

/*********************************************************************************************\
 * Data structures
\*********************************************************************************************/

struct Message {
  uint8_t type;
  uint8_t id;
  uint8_t seq;
  int32_t data;
  uint32_t padding;
  uint8_t padding1;
};

struct UDP_CLIENT_DATA {
  WiFiUDP udp;                                     // UDP client instance
  bool    initialized;                             // Initialization flag
  uint8_t buffer[UDP_BUFFER_SIZE];                // Receive buffer
  String  server_ip;                               // Server IP address
  bool    connected_to_server;                     // Connection status
  uint32_t sequence;                               // Message sequence number
  uint32_t device_id;                              // Device ID (now 32-bit for ESP Chip ID)
};

UDP_CLIENT_DATA UdpClient;

/*********************************************************************************************\
 * Function prototypes
\*********************************************************************************************/

void UdpClientInit(void);
void UdpClientLoop(void);
void UdpClientHandlePacket(void);
void UdpClientSendBroadcast(void);
void UdpClientSendPowerData(void);
void UdpClientProcessMessage(Message *msg);

/*********************************************************************************************\
 * Driver operations
\*********************************************************************************************/

void UdpClientInit(void) {
  UdpClient.initialized = false;
  UdpClient.connected_to_server = false;
  UdpClient.sequence = 1;
  // Use ESP Chip ID as unique device identifier (persistent across reboots)
  UdpClient.device_id = ESP.getChipId();  // Use full 32-bit Chip ID
  
  AddLog(LOG_LEVEL_INFO, PSTR("UDP: Using Chip ID as Device ID: 0x%08X"), UdpClient.device_id);
  UdpClient.server_ip = "";
  
  if (UdpClient.udp.begin(UDP_CLIENT_PORT)) {
    UdpClient.initialized = true;
    AddLog(LOG_LEVEL_INFO, PSTR("UDP: Client started on port %d, Device ID: 0x%08X"), UDP_CLIENT_PORT, UdpClient.device_id);
    
    // Start broadcasting immediately
    UdpClientSendBroadcast();
  } else {
    AddLog(LOG_LEVEL_ERROR, PSTR("UDP: Failed to start client on port %d"), UDP_CLIENT_PORT);
  }
}

void UdpClientLoop(void) {
  if (!UdpClient.initialized) { return; }
  
  // Handle incoming packets
  int packetSize = UdpClient.udp.parsePacket();
  if (packetSize) {
    UdpClientHandlePacket();
  }
  
  // Periodic broadcast if not connected
  static uint32_t last_broadcast = 0;
  if (!UdpClient.connected_to_server && (millis() - last_broadcast > 5000)) {
    UdpClientSendBroadcast();
    last_broadcast = millis();
  }
}

void UdpClientHandlePacket(void) {
  int len = UdpClient.udp.read(UdpClient.buffer, UDP_BUFFER_SIZE);
  if (len > 0) {
    IPAddress remoteIP = UdpClient.udp.remoteIP();
    uint16_t remotePort = UdpClient.udp.remotePort();
    
    AddLog(LOG_LEVEL_DEBUG, PSTR("UDP: Received %d bytes from %s:%d"), len, remoteIP.toString().c_str(), remotePort);
    
    // Process message if it's the right size
    if (len >= sizeof(Message)) {
      Message msg;
      memcpy(&msg, UdpClient.buffer, sizeof(Message));
      UdpClientProcessMessage(&msg);
    }
  }
}

void UdpClientProcessMessage(Message *msg) {
  switch (msg->type) {
    case CONNECT:
      AddLog(LOG_LEVEL_INFO, PSTR("UDP: Server hello received, sending broadcast response"));
      UdpClientSendBroadcast();
      break;
    case CONFIRM:
      UdpClient.connected_to_server = true;
      UdpClient.server_ip = UdpClient.udp.remoteIP().toString();
      AddLog(LOG_LEVEL_INFO, PSTR("UDP: Connected to server %s"), UdpClient.server_ip.c_str());
      break;
    case POWER_MEASUREMENT:
      AddLog(LOG_LEVEL_INFO, PSTR("UDP: Power request received, sending power data"));
      UdpClientSendPowerData();
      break;
    default:
      AddLog(LOG_LEVEL_DEBUG, PSTR("UDP: Unknown message type %d"), msg->type);
      break;
  }
}

void UdpClientSendBroadcast(void) {
  Message msg;
  msg.type = CONNECT;
  msg.id = UdpClient.device_id;
  msg.seq = UdpClient.sequence++;
  msg.data = DEV_TYPE_POWER_METER;
  msg.padding = 0;
  msg.padding1 = 0;
  
  UdpClient.udp.beginPacket(UDP_BROADCAST_IP, UDP_SERVER_PORT);
  UdpClient.udp.write((uint8_t*)&msg, sizeof(msg));
  UdpClient.udp.endPacket();
  
  AddLog(LOG_LEVEL_DEBUG, PSTR("UDP: Broadcast CONNECT sent (seq:%d)"), msg.seq);
}

void UdpClientSendPowerData(void) {
  Message msg;
  msg.type = POWER_MEASUREMENT;
  msg.id = UdpClient.device_id;
  msg.seq = UdpClient.sequence++;
  msg.data = 0;
  msg.padding = 0;
  msg.padding1 = 0;
  
#ifdef USE_ENERGY_SENSOR
  // Get current power consumption and convert to int (W * 100 for 2 decimal places)
  msg.data = (int32_t)(Energy->active_power[0] * 100);
  AddLog(LOG_LEVEL_INFO, PSTR("UDP: Sending power data: %.2fW"), Energy->active_power[0]);
#else
  // No energy sensor available
  msg.data = -1;
  AddLog(LOG_LEVEL_INFO, PSTR("UDP: No energy sensor available, sending 0W"));
#endif
  
  // Send to server (not broadcast)
  if (!UdpClient.server_ip.isEmpty()) {
    UdpClient.udp.beginPacket(UdpClient.server_ip.c_str(), UDP_SERVER_PORT);
    UdpClient.udp.write((uint8_t*)&msg, sizeof(msg));
    UdpClient.udp.endPacket();
    
    AddLog(LOG_LEVEL_DEBUG, PSTR("UDP: Power data sent to server %s (seq:%d)"), UdpClient.server_ip.c_str(), msg.seq);
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv92(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      UdpClientInit();
      break;
    case FUNC_LOOP:
      UdpClientLoop();
      break;
    case FUNC_JSON_APPEND:
      ResponseAppend_P(PSTR(",\"UDP\":{\"DeviceID\":%d,\"Connected\":%s,\"Server\":\"%s\"}"), 
        UdpClient.device_id,
        UdpClient.connected_to_server ? "true" : "false",
        UdpClient.server_ip.c_str());
      break;
    case FUNC_WEB_SENSOR:
      WSContentSend_P(PSTR("UDP: Device ID=%d, Connected=%s, Server=%s"), 
        UdpClient.device_id,
        UdpClient.connected_to_server ? "Yes" : "No",
        UdpClient.server_ip.c_str());
      break;
    case FUNC_ACTIVE:
      result = true;
      break;
  }
  return result;
}

#endif  // USE_UDP_CLIENT 