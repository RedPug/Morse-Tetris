#include <Wire.h>                // I2C library
#include <Adafruit_GFX.h>         // Core graphics library
#include <Adafruit_SSD1327.h>     // OLED display library (adjust based on your model)
#include <esp_now.h>
#include <WiFi.h>
// #include <driver/i2s.h>
// #include <arduinoFFT.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels

// Define I2C pins (defaults for ESP32)
#define SDA_PIN 11
#define SCL_PIN 13

#define numRows 20
#define numCols 10

// Create an instance of the display using I2C (OLED 128x128)
Adafruit_SSD1327 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);  // No reset pin

//define the arrays which correspond to the pieces
unsigned int objectStatic[20]; //0th index corresponds to bottom
unsigned int objectDynamic[20];


int objectDynamicRotation; //number between 0 and 3 which defines the rotation of the dynamic
int objectDynamicPositionX; //horizontal position of the dynamic
int objectDynamicPositionY; //vertical position of the dynamic


unsigned int* currentDynamic; //16 element array of the current dynamic shape


//define the list of filled rows
double fullRows;


int lastTickTime;


int points;
int gameOver;


#define TICK_SPEED 1000 //time, in ms, between ticks


#define MAX_TURN_SPEED 200 //time, in ms, between turns
int lastTurnTime;


//define game size parameters
#define numRows 20
#define numCols 10


//define pin numbers
#define JOY_PIN_X 6 //left and right
#define JOY_PIN_Y 7 //up and down


//define directions (note to self: these can NOT be zero)
#define NONE 0
#define UP 1
#define DOWN 3
#define LEFT 4
#define RIGHT 2
#define C_CLOCKWISE 5
#define CLOCKWISE 6


//define dynamic (block) shapes
//each set of four entries is a row. The next four are the clockwise rotation of the previous
  unsigned int blockI[] = {
    0, 0, B1111, 0,
    B100, B100, B100, B100,
    0, 0, B1111, 0,
    B10, B10, B10, B10
  };


  unsigned int blockO[] = {
    B11, B11, 0, 0,
    B11, B11, 0, 0,
    B11, B11, 0, 0,
    B11, B11, 0, 0
  };


  unsigned int blockT[] = {
    B10, B111, 0, 0,
    B10, B110, B10, 0,
    0, B111, B10, 0,
    B10, B11, B10,0
  };


  unsigned int blockL[] = {
    B100,B111,0,0,
    B10,B10,B110,0,
    0,B111,B1,0,
    B11,B10,B10,0
  };


  unsigned int blockJ[] = {
    B1, B111, 0, 0,
    B110, B10, B10, 0,
    0, B111, B100, 0,
    B10, B10, B11, 0
  };


  unsigned int blockS[] = {
    B110, B11, 0, 0,
    B10, B110, B100, 0,
    0, B110, B11, 0,
    B1, B11, B10, 0
  };
 
  unsigned int blockZ[] = {
    B11, B110, 0, 0,
    B100, B110, B10, 0,
    0, B11, B110, 0,
    B10, B11, B1, 0
  };
//


//create an easily indexable list of the blocks
unsigned int* blockList[] = {blockI, blockO, blockT, blockL, blockJ, blockS, blockZ};

unsigned int nextBlockNum;

typedef struct joystick_info {
  int dir;
} joystick_info;

//Create a struct_message called myData
joystick_info myData;

void setup() {
  Serial.begin(115200);         // Start serial communication for debugging

  Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C with custom SDA and SCL pins

  Serial.println("trying to connect...");

  if (!display.begin(SSD1327_I2C_ADDRESS, 0x3D)) {  // Default I2C address 0x3D
    Serial.println(F("SSD1327 display not detected"));
    while (true);  // Loop forever if the display is not detected
  }

  Serial.println("Successfully Loaded!");

  display.display();  // Initialize display
  display.clearDisplay();  // Clear the display buffer

  // Draw a white rectangle
  display.fillRect(20, 20, 60, 40, SSD1327_WHITE); // Rectangle at (20, 20) with width 60 and height 40
  display.display();  // Push the buffer to the display

  WiFi.mode(WIFI_STA);

  //Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  randomSeed(millis()*10000^3249743);
  nextBlockNum = random(0,7);
  dynamicSpawn();
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  int dir = myData.dir;
  if(dir < 0 || dir > 6){
    myData.dir = 0;
  }
}


void loop() {
  stepGame();

  display.clearDisplay();
  renderGameToBuffer();
  display.display();

  delay(100);
}

void stepGame(){
  int joyCommand = inputJoy();

  if (checkTranslation(joyCommand)) {
    dynamicMove(joyCommand);
  }


  getDynamic(); //error occured in here!!!

  if (millis() - lastTickTime > TICK_SPEED) {
    lastTickTime = millis() - millis() % TICK_SPEED; //set the latest tick start time
    gameOver = false;
    // Serial.print("Last tick time: ");
    // Serial.println(lastTickTime);
    // Serial.print("  tick check...  ");
    // delay(1000);
    // Serial.println(objectDynamicPositionX);
    if (isInsideGround()) { //if there is no space to spawn, the game resets
      gameOver = true;
      clearObjects();
      dynamicSpawn();
    }
    else if (checkTranslation(DOWN)) { //if the piece can be moved down, move it down
      dynamicMove(DOWN);
    }
    else { //if the dynamic can not be moved down, add it to the static
      // Serial.println("Merging...");
      // delay(1000);
      // Serial.println(objectDynamicPositionX);


      getDynamic();
      dynamicAdd();
      dynamicSpawn();
      // Serial.println(objectDynamicPositionX);
     
    }
  }
}


void renderGameToBuffer(){
  const int TILE_WIDTH = 6;
  const int BORDER_X = (SCREEN_WIDTH-TILE_WIDTH*numCols)/2;
  //border y pushes it all down to have room on the top!
  const int BORDER_Y = (SCREEN_HEIGHT-TILE_WIDTH*numRows);
  
  //draw score at the top.
  // Serial.println("2a");
  display.setTextSize(1);
  display.setTextColor(0xF);
  display.setCursor(0, 5);
  display.printf("Score\n%0.5d\n", points);

  //draw next block.
  // Serial.println("2b");

  const int offset = (BORDER_X - 4*TILE_WIDTH)/2;
  const int x0 = (SCREEN_WIDTH - BORDER_X) + offset;
  const int y0 = BORDER_Y + TILE_WIDTH;

  display.setTextSize(1);
  display.setTextColor(0xF);
  display.setCursor(x0, 5);
  display.print("Next");

  for (int row = 0; row < 4; row++) {
    int rowData = blockList[nextBlockNum][row];
    for (int col = 0; col < 4; col++) {
      int color = 0;
      if (rowData >> col & 1) {
        color = 0xF;
      }
      else {
        color = 0x0;
      }
      display.fillRect(x0 + col*TILE_WIDTH, y0 + (row)*TILE_WIDTH, TILE_WIDTH, TILE_WIDTH, color);
      display.drawRect(x0 + col*TILE_WIDTH, y0 + (row)*TILE_WIDTH, TILE_WIDTH, TILE_WIDTH, 0x4);
    }
  }
  
  // Serial.println("2c");
  for (int row = 0; row < numRows; row++) {
    int rowData = objectStatic[row] | objectDynamic[row];
    for (int col = 0; col < numCols; col++) {
      int color = 0;
      if (rowData >> col & 1) {
        if (objectDynamic[row] >> col & 1){
          color = 0xF;
        } else {
          color = 0x9;
        }
      }
      else {
        color = 0x0;
      }
      //fill in rect with desired color, but always have certain border on it.
      display.fillRect(BORDER_X + col*TILE_WIDTH, BORDER_Y + (numRows-row-1)*TILE_WIDTH, TILE_WIDTH, TILE_WIDTH, color);
      display.drawRect(BORDER_X + col*TILE_WIDTH, BORDER_Y + (numRows-row-1)*TILE_WIDTH, TILE_WIDTH, TILE_WIDTH, 4);
    }
  }
  // Serial.println("2d");
}



void clearObjects() {
  for (int i = 0; i < numRows; i++) {
    objectStatic[i] = 0;
    objectDynamic[i] = 0;
  }
}


//adds the dynamic to the static
void dynamicAdd() {
  getDynamic();


  //add dynamic to static
  for (int i = objectDynamicPositionY; i >= max(objectDynamicPositionY - 4, 0); i--) {
    objectStatic[i] = objectStatic[i] | objectDynamic[i];
  }


  //remove full rows
  int numClearedRows = deleteRows(); //clear the filled rows
  points += sq(numClearedRows) * 100; //award points proportional to square of number of rows eliminated in the last turn
}


//spawns the dynamic at the top of the grid
void dynamicSpawn() {
  //spawn in next dynamic
  currentDynamic = blockList[nextBlockNum]; //choose a random block to use
  objectDynamicPositionX = numCols / 2 - 2;
  objectDynamicPositionY = numRows - 1;
  objectDynamicRotation = 0;

  nextBlockNum = random(0,7);
}


//remove all of the full rows, and shift down the above blocks
int deleteRows() {
  int numClearedRows = 0;
  int j = 0;
  while (j < numRows) { //itterate through all rows and check if it is full
    if (objectStatic[j] == (1 << numCols) - 1) { //if the jth row is full, clear it
      numClearedRows++;
      for (int i = j; i + 1 < numRows; i++) { //remove the jth row and shift the rest of the blocks down
        objectStatic[i] = objectStatic[i + 1];
      }
    }
    else {
      j++;
    }
  }
  return numClearedRows;
}


//move the dynamic in the specified direction, regardless of whether it intersects with the static
void dynamicMove (int direction) {
  if (direction == DOWN) {
    objectDynamicPositionY = max(objectDynamicPositionY - 1, 0);
  }
  else if (direction == UP) {
    objectDynamicPositionY = min(objectDynamicPositionY + 1, numRows);
  }
  else if (direction == LEFT) {
    objectDynamicPositionX = max(objectDynamicPositionX - 1, -4);
  }
  else if (direction == RIGHT) {
    objectDynamicPositionX = min(objectDynamicPositionX + 1, numCols);
  }
  else if (direction == CLOCKWISE) {
    objectDynamicRotation = (objectDynamicRotation + 1) % 4;
  }
  else if (direction == C_CLOCKWISE) {
    objectDynamicRotation = (objectDynamicRotation - 1 + 4) % 4;
  }
  removeFromWall();
}


//checks if the dynamic being translated will result in a collision with the static. Returns TRUE if the move is acceptable, and FALSE when the move results in a collision
int checkTranslation (int direction) {
  if (direction == NONE) {return true;}
  //check rotations
  if (direction == CLOCKWISE || direction == C_CLOCKWISE) {
    //rotate the block
    if (direction == CLOCKWISE) {
      objectDynamicRotation = (objectDynamicRotation + 1) % 4;
    }
    else {
      objectDynamicRotation = (objectDynamicRotation - 1 + 4) % 4;
    }
    getDynamic();


    //rotate the block back to how it was
    if (direction == CLOCKWISE) {
      objectDynamicRotation = (objectDynamicRotation - 1 + 4) % 4;
    }
    else {
      objectDynamicRotation = (objectDynamicRotation + 1) % 4;
    }


    //determine if it collides with something
    int numDynamicTiles = 0;
    for (int i = objectDynamicPositionY - 4; i < objectDynamicPositionY; i++) {
      if (objectDynamic[i] & objectStatic[i]){
        return false;
      }
    }


    return true;
  }
 
 
  //define the dynamic location
  getDynamic();
  //check if the dynamic is over the static
 
  if (direction == DOWN) {
    if (objectDynamic[0]) { //if the block is at the floor, it can't move
      return false;
    }
    for (int k = 0; (int)k < numRows - 1; k++) { //for each height, check if the dynamic block is above any of the statics
      int i = k+1;
      if (i >= 20) {break;}
      // Serial.print(k); //<---- load bearing coconut
      // Serial.print(" / ");
      // Serial.print("in loop? ");
      // Serial.print(i == 20);
      // Serial.println("   ");
      if (objectStatic[k] & objectDynamic[k+1]) {
        return false;
      }
     
    }
   
  }
  //check if moving the dynamic block left or right will cause a collision with the wall
  else if (direction == LEFT) {
    for (int i = 0; i < numRows; i++) {
      if ((objectStatic[i] & (objectDynamic[i] >> 1)) || (objectDynamic[i] & 1)) {
        Serial.println("Can't move left!");
        return false;
      }
    }
  }
  else if (direction == RIGHT) {
    for (int i = 0; i < numRows; i++) {
      if ((objectStatic[i] & (objectDynamic[i] << 1)) || (objectDynamic[i] & (1 << (numCols - 1)))) {
        return false;
      }
    }
  }


  //if no errors occur, then the translation is OK
  return true;
}


//checks if the dynamic is in the wall, and move it out if it is
void removeFromWall () {
  getDynamic();
  int numTiles = 1;
  int startTime = millis();
  while (numTiles != 0) {
    int numTiles = 0;
    for (int i = 0; i < numRows; i++) {
      for (int j = 0; j < numCols; j++) {
        int row = objectDynamic[i];
        if (row & 1 << j) {
          numTiles++;
        }
      }
    }
    // Serial.print("objectTiles: ");
    // Serial.print(numTiles);


    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        int row = currentDynamic[objectDynamicRotation * 4 + i];
        if (row & 1 << j) {
          numTiles--;
        }
      }
    }
    // Serial.print("dynamic Tiles: ");
    // Serial.print(numTiles);
    if (numTiles) {
      if (objectDynamicPositionX > 0) {
        objectDynamicPositionX -= 1;
      } else {
        objectDynamicPositionX += 1;
      }
    }
    if (numTiles == 0) {
      break;
    }
    if (millis() - startTime > 50) {
      break;
    }
  }
}


void getDynamic () {
  for (int i = 0; i < numRows; i++) { //clear the dynamic object
    objectDynamic[i] = 0;
  }


  //fill in the apropriate spaces
  for (int i = 0; i < min(4, objectDynamicPositionY+1); i++) {
    //set the row in the object dynamic space to the apropriate row in the defined dynamic shape
    objectDynamic[objectDynamicPositionY - i] = currentDynamic[i + 4 * objectDynamicRotation] * pow(2, objectDynamicPositionX);
  }
}


//returns whether the dynamic is inside of the static (true if it is colliding)
int isInsideGround() {
  for (int i = 0; i < numRows; i++) {
    if (objectDynamic[i] & objectStatic[i]) {return true;}
  }
  return false;
}



//read the joystick values, and return the direction of the joystick (prioritizing horizontal (x) motion over vertical)
joystick_info pastJoyData;
int inputJoy() {
  int result = myData.dir;
  if(pastJoyData.dir == C_CLOCKWISE && myData.dir == C_CLOCKWISE){
    result = 0;
  }
  memcpy(&pastJoyData, &myData, sizeof(joystick_info));

  return result;
}

