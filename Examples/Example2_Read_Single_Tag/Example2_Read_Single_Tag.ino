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

  while (!Serial);
  Serial.println();
  Serial.println("Initializing...");

  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println("Module failed to respond. Please check wiring.");
    while (1); //Freeze!
  }

  nano.setRegion(REGION_NORTHAMERICA); //Set to North America

  nano.setReadPower(2000); //20.00 dBm.
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling
}


void loop()
{
  byte myEPC[12]; //Most EPCs are 12 bytes
  byte myEPClength;
  byte response = 0;

  while (response != RESPONSE_IS_TAGFOUND)
  {
    myEPClength = sizeof(myEPC); //We will pass this information to the function

    response = nano.readTagEPC(myEPC, myEPClength, 500); //Scan for a new tag up to 500ms
    Serial.println(F("Searching for tag"));
  }

  if (response == RESPONSE_IS_TAGFOUND)
  {
    //Print EPC
    Serial.print(F(" epc["));
    for (byte x = 0 ; x < myEPClength ; x++)
    {
      if (myEPC[x] < 0x10) Serial.print(F("0"));
      Serial.print(myEPC[x], HEX);
      Serial.print(F(" "));
    }
    Serial.println(F("]"));
  }
  else
    Serial.println("No tag detected");

  Serial.println(F("Press a key to scan for a tag"));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
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

