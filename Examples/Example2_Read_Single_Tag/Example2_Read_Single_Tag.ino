/*
  Reading multipule RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Single shot read - Ask the reader to tell us what tags it currently sees.

  If using the Simultaneous RFID Tag Reader (SRTR) shield, make sure the serial slide 
  switch is in the 'SW-UART' position
  
  Arduino pin 2 to Nano TX
  Arduino pin 3 to Nano RX
*/

#include <SoftwareSerial.h> //Used for transmitting to the device

SoftwareSerial softSerial(2, 3); //RX, TX

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //Create instance

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

  //We need to do a range test with different power settings as well. What does teh software output?


  nano.sendMessage(0x2A, 0, 0); //Clear tag ID buffer

  uint8_t blob5[] = {0x03, 0x85, 0x00};//, 0x00, 0x00};
  nano.sendMessage(0x21, blob5, sizeof(blob5)); //Read tag ID multiple
  if (msg[0] == ALL_GOOD) printResponse();
  
  //uint8_t blob4[] = {0x01, 0xFF, 0x00};
  //nano.sendMessage(0x29, blob4, sizeof(blob4)); //Get tag ID buffer
  //if (msg[0] == ALL_GOOD) printResponse();

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
//Gracefully handles a reader that is already configured and already reading continuously
boolean setupNano(long baudRate)
{
  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  nano.begin(softSerial); //Tell the library to communicate over software serial port
  nano.getVersion();

  if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a ccontinuous read
    nano.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    softSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}


