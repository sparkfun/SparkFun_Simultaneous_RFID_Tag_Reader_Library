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
  if(setupNano(38400) == false) //Configure nano to run at 57600bps
  {
    Serial.println("Module failed to respond. Please check wiring.");
    while(1); //Freeze!
  }

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  nano.setRegion(0x0D); //Set to North America

  nano.setReadPower(2700);

  nano.setTagProtocol(); //Set protocol to GEN2

  const uint8_t testID[] = "Hello World!";
  nano.writeID(testID, sizeof(testID) - 1); //The -1 shaves off the \0 found at the end of any string
  if (msg[0] == ALL_GOOD) printResponse();
  else if(msg[0] == ERROR_COMMAND_RESPONSE_TIMEOUT)
  {
    Serial.println("Response Timeout - If tag is present EPC Memory may be locked");
  }
  else
  {
    Serial.println("Failed write");
  }

  while (1);
}

void loop()
{

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

