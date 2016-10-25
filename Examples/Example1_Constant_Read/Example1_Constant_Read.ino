/*
  Reading multipule RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Constantly reads and outputs any tags heard

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
  Serial.println(F("Initializing..."));

  if (setupNano(57600) == false) //Configure nano to run at 57600bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  nano.setRegion(0x0D); //Set to North America

  nano.setReadPower(1000); //10.00 dBm. Max Read TX Power is 27.00 dBm

  nano.startReading(); //Begin scanning for tags

  Serial.println("Go!");
}

void loop()
{
  if (nano.check() == true) //Check to see if any new data has come in from module
  {
    byte response = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (response == RESPONSE_IS_KEEPALIVE)
      Serial.println(F("Keep alive received"));
    else if (response == RESPONSE_IS_TAGFOUND)
    {
      //If we have a full record we can pull out the fun bits
      int rssi = nano.getTagRSSI(); //Get the RSSI for this tag read

      long freq = nano.getTagFreq(); //Get the frequency this tag was detected at

      long timeStamp = nano.getTagTimestamp(); //Get the time this was read, (ms) since last keep-alive message

      byte tagDataBytes = nano.getTagDataBytes(); //Get the number of bytes of tag data from response

      byte tagEPCBytes = nano.getTagEPCBytes(); //Get the number of bytes of EPC from response

      Serial.print(F(" rssi["));
      Serial.print(rssi);
      Serial.print(F("]"));

      Serial.print(F(" freq["));
      Serial.print(freq);
      Serial.print(F("]"));

      Serial.print(F(" time["));
      Serial.print(timeStamp);
      Serial.print(F("]"));

      if (tagDataBytes > 0)
      {
        //Print any data bytes
        Serial.print(F(" tagData["));
        for (byte x = 0 ; x < tagDataBytes ; x++)
        {
          if (nano.msg[26 + x] < 0x10) Serial.print(F("0"));
          Serial.print(nano.msg[26 + x]);
          Serial.print(F(" "));
        }
        Serial.print(F("]"));
      }

      //Print EPC bytes
      Serial.print(F(" epc["));
      for (byte x = 0 ; x < tagEPCBytes ; x++)
      {
        if (nano.msg[31 + tagDataBytes + x] < 0x10) Serial.print(F("0"));
        Serial.print(nano.msg[31 + tagDataBytes + x]);
        Serial.print(F(" "));
      }
      Serial.print(F("]"));

      Serial.println();
    }
  }
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

