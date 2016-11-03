/*
  Reading multipule RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Write new data to the user data area
  Some tags have 64, 16 4, or 0 bytes of user data available for writing.

  If you write more bytes than is available (10 bytes and only 4 available) module will simply timeout.

  EPC is good for things like UPC (this is a gallon of milk)
  User data is a good place to write things like the milk's best by date

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

  //Warning! Writing to a tag causes module to go to max power
  //An external power supply is required. Powering from USB alone will cause the sketch to reboot randomly.
}

void loop()
{
  Serial.println();
  Serial.println(F("Get all tags out of the area. Press a key to write DATA to first detected tag."));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character

  //"Hello" Does not work. "Hell" will be recorded. You can only write even number of bytes
  char testData[] = "ACBDEF"; //You can only write even number of bytes
  byte response = nano.writeTagData(testData, sizeof(testData) - 1); //The -1 shaves off the \0 found at the end of string

  if (response == RESPONSE_IS_WRITE_SUCCESS)
    Serial.println("New Data Written!");
  else
  {
    Serial.println();
    Serial.println("Failed write");
    Serial.println("Did you write too much data?");
    Serial.println("Is the tag locked?");
  }
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

