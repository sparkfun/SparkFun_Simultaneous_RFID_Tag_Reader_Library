/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Don't get too excited, this only reads the passwords if they are unlocked.

  There are two passwords associated with any given tag: the Kill PW and the Acess PW
*/

// Library for controlling the RFID module
#include "SparkFun_UHF_RFID_Reader.h"

// Create instance of the RFID module
RFID rfidModule;

// By default, this example assumes software serial. If your platform does not
// support software serial, you can use hardware serial by commenting out these
// lines and changing the rfidSerial definition below
#include <SoftwareSerial.h>
SoftwareSerial softSerial(2, 3); //RX, TX

// Here you can specify which serial port the RFID module is connected to. This
// will be different on most platforms, so check what is needed for yours and
// adjust the definition as needed. Some examples are provided below
#define rfidSerial softSerial // Software serial (eg. Arudino Uno or SparkFun RedBoard)
// #define rfidSerial Serial1 // Hardware serial (eg. ESP32 or Teensy)

// Here you can select the baud rate for the module. 38400 is recommended if
// using software serial, and 115200 if using hardware serial.
#define rfidBaud 38400
// #define rfidBaud 115200

// Here you can select which module you are using. This library was originally
// written for the M6E Nano only, and that is the default if the module is not
// specified. Support for the M7E Hecto has since been added, which can be
// selected below
#define moduleType ThingMagic_M6E_NANO
// #define moduleType ThingMagic_M7E_HECTO

void setup()
{
  Serial.begin(115200);

  while (!Serial);
  Serial.println();
  Serial.println("Initializing...");

  if (setupRfidModule(rfidBaud) == false)
  {
    Serial.println("Module failed to respond. Please check wiring.");
    while (1); //Freeze!
  }

  rfidModule.setRegion(REGION_NORTHAMERICA); //Set to North America

  rfidModule.setReadPower(500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling
}

void loop()
{
  Serial.println(F("Get all tags out of the area. Press a key to read PWs from first detected tag."));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character

  byte response;
  byte myPW[4];
  byte pwLength = sizeof(myPW);
  
  //Read Kill password
  response = rfidModule.readKillPW(myPW, pwLength);
  if (response == RESPONSE_SUCCESS)
  {
    Serial.println("PW read!");
    Serial.print("KillPW: [");
    for(byte x = 0 ; x < pwLength ; x++)
    {
      if(myPW[x] < 0x10) Serial.print("0");
      Serial.print(myPW[x], HEX);
      Serial.print(" ");
    }
    Serial.println("]");
  }
  else
    Serial.println("Failed read");

  
  //Read Access PW
  pwLength = sizeof(myPW); //Reset this variable. May have been changed above.
  response = rfidModule.readAccessPW(myPW, pwLength);
  if (response == RESPONSE_SUCCESS)
  {
    Serial.println("PW read!");
    Serial.print("AccessPW: [");
    for(byte x = 0 ; x < pwLength ; x++)
    {
      if(myPW[x] < 0x10) Serial.print("0");
      Serial.print(myPW[x], HEX);
      Serial.print(" ");
    }
    Serial.println("]");
  }
  else
    Serial.println("Failed read");
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupRfidModule(long baudRate)
{
  rfidModule.begin(rfidSerial, moduleType); //Tell the library to communicate over serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  rfidSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  delay(100); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while (rfidSerial.available())
    rfidSerial.read();

  rfidModule.getVersion();

  if (rfidModule.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a ccontinuous read
    rfidModule.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    rfidSerial.begin(115200); //Start serial at 115200

    rfidModule.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    rfidSerial.begin(baudRate); //Start the serial port, this time at user's chosen baud rate

    delay(250);
  }

  //Test the connection
  rfidModule.getVersion();
  if (rfidModule.msg[0] != ALL_GOOD)
    return false; //Something is not right

  //The module has these settings no matter what
  rfidModule.setTagProtocol(); //Set protocol to GEN2

  rfidModule.setAntennaPort(); //Set TX/RX antenna ports to 1

  return true; //We are ready to rock
}
