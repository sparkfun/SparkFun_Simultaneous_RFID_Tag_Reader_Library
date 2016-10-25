/*
  Reading multipule RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Write a new EPC (Electronic Product Code) to a tag
  This is a good way to assign your own, easy to read ID to a tag.

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
  if(setupNano(9600) == false) //Configure nano to run at 57600bps
  {
    Serial.println("Module failed to respond. Please check wiring.");
    while(1); //Freeze!
  }

  int amt = 10;
  //All of these are probably not needed
  nano.sendMessage(0x03, 0, 0); //Get HW version
  delay(amt);
  nano.sendMessage(0x0C, 0, 0); //Get current prog
  delay(amt);
  nano.sendMessage(0x68, 0, 0); //Get power mode
  delay(amt);
  nano.setBaud(9600);
  delay(amt);
  nano.getOptionalParameters(0x01, 0x0E); //Get optional params
  delay(amt);
  nano.getOptionalParameters(0x01, 0x12); //Get optional params
  delay(amt);
  
  uint8_t blob1[] = {0x05};
  nano.sendMessage(0x61, blob1, sizeof(blob1)); //Get antenna port
  delay(amt);
  
  nano.getOptionalParameters(0x01, 0x0C); //Get optional params
  delay(amt);
  nano.getOptionalParameters(0x01, 0x0D); //Get optional params
  delay(amt);

  nano.sendMessage(0x63, 0, 0); //Get tag protocol
  delay(amt);
  nano.sendMessage(0x67, 0, 0); //Get region
  delay(amt);
  nano.sendMessage(0x71, 0, 0); //Get available regions
  delay(amt);

  nano.setRegion(0x0D); //Set to North America
  delay(amt);

  uint8_t blob2[] = {0x01};
  nano.sendMessage(0x62, blob2, sizeof(blob2)); //Get read power with limits
  delay(amt);
  nano.setReadPower(2700);
  delay(amt);
  nano.sendMessage(0x62, blob2, sizeof(blob2)); //Get read power with limits
  delay(amt);
  nano.sendMessage(0x65, 0, 0); //Get hop table
  delay(amt);
  nano.sendMessage(0x62, blob2, sizeof(blob2)); //Get read power with limits
  delay(amt);
  nano.sendMessage(0x62, blob2, sizeof(blob2)); //Get read power with limits
  delay(amt);

  nano.getProtocolParameters(0x05, 0x10); //Get protocol params
  delay(amt);
  nano.getProtocolParameters(0x05, 0x11); //Get protocol params
  delay(amt);
  nano.getProtocolParameters(0x05, 0x02); //Get protocol params
  delay(amt);
  nano.getProtocolParameters(0x05, 0x00); //Get protocol params
  delay(amt);
  nano.getProtocolParameters(0x05, 0x01); //Get protocol params
  delay(amt);
  nano.getProtocolParameters(0x05, 0x12); //Get protocol params
  delay(amt);

  nano.getReadPower(); //Don't think this is needed? Read the current power setting
  delay(amt);

  nano.getOptionalParameters(0x01, 0x04); //Get optional params
  delay(amt);
  nano.getOptionalParameters(0x01, 0x04); //Get optional params
  delay(amt);

  nano.sendMessage(0x61, blob1, sizeof(blob1)); //Get antenna port
  delay(amt);
  nano.sendMessage(0x61, blob1, sizeof(blob1)); //Get antenna port
  delay(amt);

  nano.sendMessage(0x70, 0, 0); //Get available protocols
  delay(amt);

  uint8_t blob3[] = {0x00, 0x00};
  nano.sendMessage(0x10, blob3, sizeof(blob3)); //HW version (not get, not set)
  delay(amt);

  nano.sendMessage(0x2A, 0, 0); //Clear tag ID buffer
  delay(amt);

  //nano.setWritePower(2700); //Don't think this is needed?


  nano.setOptionalParameters();
  //nano.setOptionalParameters(); //Probably not needed

  nano.setProtocolParameters();
  //nano.setOptionalParameters(); //Probably not needed

  nano.setAntennaSearchList(); //Probably not needed
  delay(amt);

  uint8_t blob6[] = {0x00, 0x05};
  nano.sendMessage(TMR_SR_OPCODE_SET_TAG_PROTOCOL, blob6, sizeof(blob6)); //Set protocol

  uint8_t blob5[] = {0x00, 0x00, 0x13, 0x01, 0xF4};
  nano.sendMessage(0x22, blob5, sizeof(blob5)); //Read tag ID multiple
  if (msg[0] == ALL_GOOD) printResponse();
  delay(amt);
  
  uint8_t blob4[] = {0x01, 0xFF, 0x00};
  nano.sendMessage(0x29, blob4, sizeof(blob4)); //Get tag ID buffer
  if (msg[0] == ALL_GOOD) printResponse();
  delay(amt);

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1
  delay(amt);

  //const uint8_t testID[] = "Hello World!";
  //const uint8_t testID[] = "test";
  //nano.writeID(testID, sizeof(testID) - 1); //The -1 shaves off the \0 found at the end of any string
  uint16_t testID = 0xAABB;
  nano.writeID(testID, sizeof(testID)); //The -1 shaves off the \0 found at the end of any string
  if (msg[0] == ALL_GOOD) printResponse();
  else Serial.println("Failed write");

  delay(100);

  nano.writeID(testID, sizeof(testID) - 1); //The -1 shaves off the \0 found at the end of any string
  if (msg[0] == ALL_GOOD) printResponse();
  else Serial.println("Failed write");

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

