/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  An example showing how to read the unique ID from a tag.
  
  Every tag has a Tag ID (the TID) that is not editable. This memory
  contains the chip vendor ID and model ID for the tag (24 bits). All tags
  must have these bits. Almost all tags also include a UID or unique ID
  in the TID memory area. 

  UIDs can range in length. We've seen 8 bytes (64 bit) most commonly but some
  UIDs are 20 bytes, 160 bits or 1,460,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000 
  different possible unique IDs

  UIDs can range from 0 to 20 bytes. If the sketch tries to read a UID
  that is longer than a tag actually has then the read will fail. Start with 8 and increase it
  if you have a tag with a longer UID.
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

  rfidModule.enableDebugging(); //Turns on commands sent to and heard from RFID module
}

void loop()
{
  Serial.println(F("Get one tag near the reader. Press a key to read a tag's unique ID."));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character

  byte response;
  byte myUID[8]; //UIDs can range from 0 to 20 bytes. Start with 8.
  byte uidLength = sizeof(myUID);
  
  //Read unique ID of tag
  response = rfidModule.readTID(myUID, uidLength);
  if (response == RESPONSE_SUCCESS)
  {
    Serial.println("UID read!");
    Serial.print("UID: [");
    for(byte x = 0 ; x < uidLength ; x++)
    {
      if(myUID[x] < 0x10) Serial.print("0");
      Serial.print(myUID[x], HEX);
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
