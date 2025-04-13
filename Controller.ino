#include <esp_now.h>
#include <WiFi.h>

//create the morse code library
const int morseCode[] = {12,2111,2121,211,1,1121,221,1111,11,1222,212,1211,22,21,222,1221,2212,121,111,2,112,1112,122,2112,2122,2211};


#define FREQUENCY 600
#define SPEAKER_PIN 5
#define UNIT_LENGTH 60 //time in ms of each dit


#define JOY_PIN_Y 6
#define JOY_PIN_X 7

#define NONE 0
#define UP 1
#define DOWN 3
#define LEFT 4
#define RIGHT 2
#define C_CLOCKWISE 5
#define CLOCKWISE 6

int lastTurnTime = 0;
int previousDir = -1;
#define MAX_TURN_SPEED 200;


// REPLACE WITH YOUR ESP RECEIVER'S MAC ADDRESS
uint8_t broadcastAddress[] = {0x8C,0xBF, 0xEA, 0x03, 0xB3, 0x7C};

typedef struct joystick_info {
  int dir;
} joystick_info;

joystick_info joyData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  Serial.print("Packet to: ");
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" send status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  Serial.begin(115200);
 
  WiFi.mode(WIFI_STA);
 
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(OnDataSent);
   
  // register peer
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // register first peer  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}
 
void loop() {
  int myDir = inputJoy();
  if (myDir != previousDir) {
    joyData.dir = myDir;
    Serial.print(myDir);
    char outputCommand = myDir + (int)'a';
    playMorse(outputCommand);
    Serial.println(outputCommand);
  
    esp_err_t result = esp_now_send(0, (uint8_t *) &joyData, sizeof(joystick_info));
    
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
  }
  delay(50);
  previousDir = myDir;
}

//read the joystick values, and return the direction of the joystick (prioritizing horizontal (x) motion over vertical)
int inputJoy() {
  const int MID_VALUE = 4096 / 2; //define the middle, centered, value
  
  int joyX = analogRead(JOY_PIN_X); //left/right axis
  int joyY = analogRead(JOY_PIN_Y); //up/down axis

  // Serial.print(joyX); Serial.print(" "); Serial.println(joyY);
  if (joyX > MID_VALUE * 1.3) {
    return RIGHT;
  }
  else if (joyX < MID_VALUE * 0.7) {
    return LEFT;
  }
  else if (joyY > MID_VALUE * 1.3) {
    // if (millis() - lastTurnTime > 200) {
    //   lastTurnTime = millis();
    //   return C_CLOCKWISE;
    // }
    // lastTurnTime = millis();
    // return NONE;
    return C_CLOCKWISE;
  }
  else if (joyY < MID_VALUE * 0.7) {
    return DOWN;
  }
  else {
    return NONE;
  }
}

//play the given character in morse code, on the speaker
void playMorse(char letter) {
  int keyPhrase = morseCode[letter - 97];
  if (letter == ' ') {
    noTone(SPEAKER_PIN);
    delay(UNIT_LENGTH * 5);
    return;
  }
  for (int i = 0; i < 4; i++) {
    int signalType = (keyPhrase * pow(10.0,-i));
    signalType = signalType % 10;
    switch (signalType){
      case 1: //dit
        tone(SPEAKER_PIN, FREQUENCY);
        delay(UNIT_LENGTH);
        break;
      case 2: //dah
        tone(SPEAKER_PIN, FREQUENCY);
        delay(UNIT_LENGTH * 3);
        break;
      
    }
    noTone(SPEAKER_PIN);
    if (signalType) {
      delay(UNIT_LENGTH);
    }
  }
  delay(UNIT_LENGTH * 2);
}
