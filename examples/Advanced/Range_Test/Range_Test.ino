/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Constantly reads. Emits a high tone beep when a tag is detected, and a low tone beep
  when no tags are detected. This is useful for testing the read range of your setup.

  Use an external power supply at set to max read power.

  Note: Humans are basically bags of water. If you hold the tag in your hand you'll
  degrade the range significantly (you meatbag, you). Tape the tag to a rolling chair
  or other non-metal, non-watery device.

  If using the Simultaneous RFID Tag Reader (SRTR) shield, make sure the serial slide
  switch is in the 'SW-UART' position
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

#define BUZZER1 10
//#define BUZZER1 0 //For testing silently
#define BUZZER2 9

boolean tagDetected; //Keeps track of when we've beeped
long lastSeen = 0; //Tracks the time when we last detected a tag
int counter = 0; //Tracks how many times we've read a tag

void setup()
{
  Serial.begin(115200);

  pinMode(BUZZER1, OUTPUT);
  pinMode(BUZZER2, OUTPUT);

  digitalWrite(BUZZER2, LOW); //Pull half the buzzer to ground and drive the other half.

  while (!Serial); //Wait for the serial port to come online

  if (setupRfidModule(rfidBaud) == false)
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  rfidModule.setRegion(REGION_NORTHAMERICA); //Set to North America

  //rfidModule.setReadPower(500); //Limited read range
  rfidModule.setReadPower(2700); //You'll need an external power supply for this setting
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  rfidModule.startReading(); //Begin scanning for tags

  Serial.println("Go!");

  lowBeep(); //Indicate no tag found
  tagDetected = false;
}

void loop()
{
  if (rfidModule.check() == true) //Check to see if any new data has come in from module
  {
    byte responseType = rfidModule.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_TAGFOUND)
    {
      Serial.print(F("Tag detected: "));
      Serial.println(counter++);

      if (tagDetected == false) //Beep if we've detected a new tag
      {
        tagDetected = true;
        highBeep();
      }
      else if (millis() - lastSeen > 250) //Beep every 250ms
      {
        highBeep();
      }
      lastSeen = millis();

    }
  }

  if (tagDetected == true && (millis() - lastSeen) > 1000)
  {
    Serial.println(F("No tag found..."));

    tagDetected = false;
    lowBeep();
  }

  //If user presses a key, pause the scanning
  if (Serial.available())
  {
    rfidModule.stopReading(); //Stop scanning for tags

    Serial.read(); //Throw away character
    Serial.println("Scanning paused. Press key to continue.");
    while (!Serial.available());
    Serial.read(); //Throw away character

    rfidModule.startReading(); //Begin scanning for tags
  }
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

void lowBeep()
{
  tone(BUZZER1, 130, 150); //Low C
  //delay(150);
}

void highBeep()
{
  tone(BUZZER1, 2093, 150); //High C
  //delay(150);
}

