/*
  Reading multipule RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  This is a stripped down implementation of the Mercury API from ThingMagic

  Module powers on at 115200bps by default. We can change baud rate to 9600bps but it is not saved to memory.


  Examples:
  Just read tags
  Write a new ID to a tag
  Write to user memory
  Kill tag


  Arduino pin 2 to Nano RX
  Arduino pin 3 to Nano TX
*/

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //Create instance

#include <SoftwareSerial.h> //Used for transmitting to the device

SoftwareSerial softSerial(3, 2); //RX, TX

//This is our universal msg array, used for all communication
//Before sending a command to the module we will write our command and CRC into it
//And before returning we will record the response into the msg array
//By default it is 255 bytes. Defined in SparkFun_UHF_RFID_Reader.h
uint8_t msg[MAX_MSG_SIZE];

void setup()
{
  Serial.begin(115200);

  while(!Serial);
  Serial.println();
  Serial.println("Initializing...");

  //57600 works well except for large comms like reading the freq hop table (205 bytes)
  //38400 works with freq hop table reading
  //9600 may be too slow for reading lots of tags simultaneously
  if(setupNano(38400) == false) //Configure nano to run at 57600bps
  {
    Serial.println("Module failed to respond. Please check wiring.");
    while(1); //Freeze!
  }

  nano.setRegion(0x0D); //Set to North America

  nano.setReadPower(2700);

  nano.sendMessage(0x2A, 0, 0); //Clear tag ID buffer

  //nano.setWritePower(2700); //Don't think this is needed?

  //nano.setOptionalParameters();
  //nano.setOptionalParameters(); //Probably not needed

  //nano.setProtocolParameters();
  //nano.setOptionalParameters(); //Probably not needed

  nano.setAntennaSearchList(); //Probably not needed

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  uint8_t blob3[] = {0x00, 0x00};
  nano.sendMessage(0x10, blob3, sizeof(blob3)); //HW version (not get, not set)

  nano.sendMessage(0x2A, 0, 0); //Clear tag ID buffer

  nano.setAntennaSearchList(); //Probably not needed

  uint8_t blob5[] = {0x00, 0x00, 0x13, 0x01, 0xF4};
  nano.sendMessage(0x22, blob5, sizeof(blob5)); //Read tag ID multiple
  if (msg[0] == ALL_GOOD) printResponse();
  
  uint8_t blob4[] = {0x01, 0xFF, 0x00};
  nano.sendMessage(0x29, blob4, sizeof(blob4)); //Get tag ID buffer
  if (msg[0] == ALL_GOOD) printResponse();

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  
  //nano.killTag(0xAABBCCDD);
  nano.killTag(0x00);
  if (msg[0] == ALL_GOOD) printResponse();
  
  while (1);

  
  
  delay(1000);

  //const uint8_t testID[] = "Hello World!";
  /*const uint8_t testID[] = "test";
  nano.writeID(testID, sizeof(testID) - 1); //The -1 shaves off the \0 found at the end of any string
  if (msg[0] == ALL_GOOD) printResponse();
  else Serial.println("Failed write");*/

  uint8_t myEPC[] = {0xAA, 0xBB};
  nano.readUserData(myEPC, sizeof(myEPC));  
  if (msg[0] == ALL_GOOD) printResponse();
  else Serial.println("Failed read");

  while (1);

  nano.startReading(); //Begin scanning for tags

  Serial.println("Go!");
}

void loop()
{
  if (nano.check() == true) //Check to see if any new data has come in from module
  {
    nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp
  }
}

//Print the current MSG array
void printResponse()
{
  Serial.print("response: ");
  for (uint8_t x = 0 ; x < msg[1] + 7 ; x++)
  {
    Serial.print(" [");
    if (msg[x] < 0x10) Serial.print("0");
    Serial.print(msg[x], HEX);
    Serial.print("]");
  }
  Serial.println();
}

//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  nano.begin(msg, softSerial); //Tell the library to communicate over software serial port
  nano.getVersion();
  if (msg[0] == ALL_GOOD) return(true); //If module responded at this baud rate then we're done!

  //If we've gotten this far, the module did not respond so assume it's just been powered on
  //and communicating at 115200bps
  
  softSerial.begin(115200); //Start software serial at 115200

  nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

  softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate

  //Test the connection
  nano.getVersion();
  if (msg[0] == ALL_GOOD) return(true); //If module responded at this baud rate then we're done!
  else return(false);
}

