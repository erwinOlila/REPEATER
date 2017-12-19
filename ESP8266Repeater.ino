/**
The code for the repeater device. This device extends connection of server
devices in the field layer to the database by publishing the message sent by
RFID-WSA devices to the raspberry pi by MQTT.

The repeater constantly listens incomming message from the servers in the field
layer. If a message, euther occupany staus or load status, is ready, the
client[i].available is fired as TRUE and the message is read per byte. The
message is then pulblished to the database by MQTT.

On the other hand, the repeater constantly subscribes the topic: "control" under
MQTT protocol. When a control message is published from the client in the
database, the message is automatically sent to the repeater. While the repeater
is connected with a server, the message is sent by "client[i].print" in the
process.
**/

#include <ESP8266WiFi.h>
#include <MyCommonFun.h>
#include <PubSubClient.h>

#define IND 4

// Indicate WiFi parameters of the raspberry pi
const char* ssidRpi = "RaspberryPi3AP";
const char* passRpi = "raspberryroom365";
const byte serverRpi[] = {172, 24, 1, 1};

// Initialize message arrays
const char sta[] = "STA";
const char occ[] = "OCC";
char building[] = "bunzel";
char messageBuff[] = "0000000";
char clusCon[] = "111";      // Initialize state of clasrooms under this cluster
char haltComm[] = "111";    // Array that monitors if forced stop is invoked

WiFiServer server(PORT); // Create instance for server
WiFiClient client[MAX_CLIENTS];
WiFiClient espClient; // Create instance for MQTT client

/**
* This function is fired when a message from the database is receved
* @params topic: pointer to topic array
* @params payload: pointer to the message array
* @params length: int - length of message
**/
void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;
  // Read message per byte
  for (i = 0; i < length; i++) {
    messageBuff[i] = payload[i];
  }
  // IF force stop is invoked, turn off
  if (messageBuff[6] == 'X') {
    // If message is for LB343/346
    if (messageBuff[4] == CLUSROOMS[0]) {
      haltComm[0] = '0';
    }
/**************************Continued on the next page**************************/
    // If message is for LB344/347
    else if (messageBuff[4] == CLUSROOMS[1]) {
      haltComm[1] = '0';
    }
    // If message is for LB345/LB348
    else {
      haltComm[2] = '0';
    }
  }

  // If forced stop is not invoked
  else if (messageBuff[6] == 'Y') {
    if (messageBuff[4] == CLUSROOMS[0]) {
      haltComm[0] = '1';
    }
    else if (messageBuff[4] == CLUSROOMS[1]) {
      haltComm[1] = '1';
    }
    else {
      haltComm[2] = '1';
    }
  }
  else {
    if (messageBuff[4] == CLUSROOMS[0]) {
      if (haltComm[0] == '0') {
        clusCon[0] = '0';
      }
      else {
        clusCon[0] = messageBuff[6];
      }
    }
    if (messageBuff[4] == CLUSROOMS[1]) {
      if (haltComm[1] == '0') {
        clusCon[1] = '0';
      }
      else {
        clusCon[1] = messageBuff[6];
      }
    }
    if (messageBuff[4] == CLUSROOMS[2]) {
      if (haltComm[2] == '0') {
        clusCon[2] = '0';
      }
      else {
        clusCon[2] = messageBuff[6];
      }
    }
  }
  Serial.print("Message from broker: ");
  Serial.println(messageBuff);
  Serial.println("Message after halt: ");
  Serial.println(clusCon);
}

// Create instance for MQTT client
PubSubClient clientPi(serverRpi, PORTPI, callback, espClient);
/**************************Continued on the next page**************************/
char message[] = "000000000000000000000000"; // Handles message from servers
char pubTopic[] = "00000000000000000";       // Array that handles topic
int i = 0;
int c = 0;

// Topic object
typedef struct messageTopic {
  char messInd[4] = "000";  // STA/OCC
  char roomLoc[6] = "00000"; // room number
  char load[4] = "000";
  //if room is occupied, this will get the rfid of the occupant
  char rfid[9] = "00000000";
  //if room is unoccupied, this will get the message 'OUT'
  char out[4] = "000";
  char loadLoc[2] = "0";
}Topic;

void setup () {
  Serial.begin(115200);
  pinMode(IND, OUTPUT);
  digitalWrite(IND, LOW); //GPIO2 is source mode so it should be set LOW to turn
                        //on the built-in LED
  WiFi.mode (WIFI_AP_STA); // Set asboth access point and station
  WiFi.softAP(ssid, pass);
  WiFi.softAPConfig(ip, gateway, subnet);
  server.begin();
  Serial.print("WiFi is created with IP: ");
  IPAddress myIP = WiFi.softAPIP();
  Serial.println(myIP);

  // Connect to raspberry pi
  WiFi.begin(ssidRpi, passRpi);
  conBuffer(ssidRpi,passRpi, IND);
  clientPi.connect(ESP8266Client); // Connect as MQTT client
  clientPi.subscribe(subtopic); // Subscribe to topic
}

void loop () {
  clientReset(); // Resets all connections
  // If connection is cut, reset connection
  if (WiFi.status() != WL_CONNECTED) {
    conBuffer(ssidRpi, passRpi, IND);
    clientPi.connect(ESP8266Client);
    clientPi.subscribe(subtopic);
  }
  clientPi.loop(); // Maintains MQTT connection

  // Set condition if no client has connected
  if(!client[0] && !client[1] && !client[2] && !client[3]){
    digitalWrite(IND, LOW);
    delay(500);
    return;
  }
/**************************Continued on the next page**************************/
  // Lopp through connected clients
  for (i = 0; i < MAX_CLIENTS-1; i++) {
    // If readable data is available
    if (client[i].available()) {
      Serial.println("Communicating Client...");
      digitalWrite(IND, HIGH);
      while (client[i].available()>0) {
        char *unitChar = message;
        *(unitChar+c)= client[i].read(); // Read data per byte
        c++;      // c becomes the size of received message but without the '\0'
      }
      client[i].print(clusCon); // Send control comand
    messageDecode(message, c); // Process message before sending to database
    messageReset(message, sizeof(message)); // Reset message
    }
    delay(500);
  }
  messageReset(pubTopic, sizeof(pubTopic));
}
/**
* Processes the message before sending to the database. This assemsles the topic
* based on the message received.
* @params messD: pointer to array that handles the message from servers
* @params lens: int - length of message
**/
void messageDecode(char* messD, int lens) {
  //lens here is the number of elements without '\0'
  int mIn = 0;                       //Index for message decoding

  Topic mqtTopic;
  Topic* mqtPtr = &mqtTopic;
  strcpy(pubTopic, building); // pubTopic = "bunzel"
  strcat(pubTopic, "/");      // pubTopic = "bunzel/"

  // Getting the room number
  for (mIn = 0; mIn < ROOMNLEN; mIn++) {
    mqtPtr -> roomLoc[mIn] = *(messD + mIn);
  }
  strcat(pubTopic, mqtPtr -> roomLoc); // pubTopic = "bunzel/LB34?"
  strcat(pubTopic, "/");    // pubTopic = "bunzel/LB34?/"

  Serial.print("Message from RFID reader: ");
  Serial.println(message);

  for (mIn = 0; mIn < 3; mIn++) {
    mqtPtr -> messInd[mIn] = *(messD + (lens-(3-mIn)));  // Extract  STA or OCC
  }

  int staOcc = strMatch(mqtPtr -> messInd, mIn); // if 1: STA, if 2: OCC else: 0
  if (staOcc == STA) {
    // Loop through the middle string where the length depends on the number of
    // load status sent, max of 4
    strcat(pubTopic, mqtPtr -> messInd);
    publishToRpi(pubTopic, message);
  }
/**************************Continued on the next page**************************/
  if (staOcc == OCC) {
    strcat(pubTopic, mqtPtr -> messInd);
    publishToRpi(pubTopic, message);
  }
  c = 0;              // Reset c for new incoming messages from incoming clients
}

/**
* Check each element of the messInd if it matches either STA or OCC
* @params ind: pointer to array that handles message indicator
* params lens: int - size of message
**/
int strMatch(char* ind, int lens) {
  int m = 0; int n = 0;
  for (i = 0; i < lens; i++) {

    if(*(ind + i) == sta[i]) {
      m++;
    }
    if(*(ind + i) == occ[i]) {
      n++;
    }
  }

  if (m == lens) {
    return STA;      //return 1 if STA
  }
  if (n == lens) {
    return OCC;      //return 2 if OCC
  }
  else {
    return 0;      //return none if neither
  }

}

// Reset connections
void clientReset () {
  int i = 0;
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (client[i]) {
      client[i].flush();
      client[i].stop();
    }
    client[i] = server.available();
  }
}

/**
* Publishing the message to raspberry pi
* @params topicToPub: pointer to array that handles the final topic name
* @params messageToPub: pointer to array that handles the final message
**/
void publishToRpi (char* topicToPub, char* messageToPub) {
  clientPi.connect(ESP8266Client);
  clientPi.publish(topicToPub, messageToPub);
}
/*************************************End**************************************/
