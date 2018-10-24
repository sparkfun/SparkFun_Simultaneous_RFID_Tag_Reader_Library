/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Write a new EPC (Electronic Product Code) to a tag
  This is a good way to assign your own, easy to read ID to a tag.
  Most tags have 12 bytes available for EPC

  This example is written for the Teensy and demonstrates how to 
  use a hardware serial port - Serial5 in this example.

  EPC is good for things like UPC (this is a gallon of milk)
  User data is a good place to write things like the milk's best by date
*/

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //Create instance

void setup()
{
  Serial.begin(115200);

  while (!Serial);
  Serial.println();
  Serial.println("Initializing...");

  //Because we are using a hardware serial port in this example we can
  //push the serial speed much faster to 115200bps
  if (setupNano(115200) == false) //Configure nano to run at 115200bps
  {
    Serial.println("Module failed to respond. Please check wiring.");
    while (1); //Freeze!
  }

  nano.setRegion(REGION_NORTHAMERICA); //Set to North America

  nano.setReadPower(500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  nano.setWritePower(500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Write TX Power is 27.00 dBm and may cause temperature-limit throttling
}

void loop()
{
  Serial.println(F("Get all tags out of the area. Press a key to write EPC to first detected tag."));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character

  //"Hello" Does not work. "Hell" will be recorded. You can only write even number of bytes
  char stringEPC[] = "Hello!"; //You can only write even number of bytes
  byte responseType = nano.writeTagEPC(stringEPC, sizeof(stringEPC) - 1); //The -1 shaves off the \0 found at the end of string

  //char hexEPC[] = {0xFF, 0x2D, 0x03, 0x54}; //You can only write even number of bytes
  //byte response = nano.writeTagEPC(hexEPC, sizeof(hexEPC));

  if (responseType == RESPONSE_SUCCESS)
    Serial.println("New EPC Written!");
  else
    Serial.println("Failed write");
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.enableDebugging(Serial); //Print the debug statements to the Serial port
  
  //nano.begin(softSerial); //Tell the library to communicate over software serial port
  nano.begin(Serial5); //Tell the library to communicate over Teensy Serial Port # 5 (pins 33/34)

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  
  //Software serial
  //softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  //while (softSerial.isListening() == false); //Wait for port to open

  //Hardware serial
  Serial5.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while(!Serial5);

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  //while (softSerial.available()) softSerial.read();
  while (Serial5.available()) Serial5.read();

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
    //softSerial.begin(115200); //Start software serial at 115200
    Serial5.begin(115200); //Start serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    //softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
    Serial5.begin(baudRate); //Start the serial port, this time at user's chosen baud rate

    delay(250);
  }

  //Test the connection
  nano.getVersion();

  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}

