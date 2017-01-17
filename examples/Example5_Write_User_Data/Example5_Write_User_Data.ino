/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Write new data to the user data area
  Some tags have 64, 16 4, or 0 bytes of user data available for writing.

  If you write more bytes than is available (10 bytes and only 4 available) module will simply timeout.

  EPC is good for things like UPC (this is a gallon of milk)
  User data is a good place to write things like the milk's best by date
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

  nano.setReadPower(500); //5.00 dBm. Higher values may caues USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  nano.setWritePower(900); //9.00 dBm. Higher values may caues USB port to brown out
  //Max Write TX Power is 27.00 dBm and may cause temperature-limit throttling
}

void loop()
{
  Serial.println();
  Serial.println(F("Get all tags out of the area. Press a key to write DATA to first detected tag."));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character

  //"Hello" is recorded as "Hell". You can only write even number of bytes
  char testData[] = "ACBD"; //You can only write even number of bytes
  byte responseType = nano.writeUserData(testData, sizeof(testData) - 1); //The -1 shaves off the \0 found at the end of string

  if (responseType == RESPONSE_SUCCESS)
    Serial.println("New Data Written!");
  else
  {
    Serial.println();
    Serial.println("Failed write");
    Serial.println("Did you write more data than the tag has memory?");
    Serial.println("Is the tag locked?");
  }
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while(!softSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while(softSerial.available()) softSerial.read();
  
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

