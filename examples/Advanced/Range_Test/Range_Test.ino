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

#include <SoftwareSerial.h> //Used for transmitting to the device

SoftwareSerial softSerial(2, 3); //RX, TX

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //Create instance

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

  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  nano.setRegion(REGION_NORTHAMERICA); //Set to North America

  //nano.setReadPower(500); //Limited read range
  nano.setReadPower(2700); //You'll need an external power supply for this setting
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  nano.startReading(); //Begin scanning for tags

  Serial.println("Go!");

  lowBeep(); //Indicate no tag found
  tagDetected = false;
}

void loop()
{
  if (nano.check() == true) //Check to see if any new data has come in from module
  {
    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

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
    nano.stopReading(); //Stop scanning for tags

    Serial.read(); //Throw away character
    Serial.println("Scanning paused. Press key to continue.");
    while (!Serial.available());
    Serial.read(); //Throw away character

    nano.startReading(); //Begin scanning for tags
  }
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //nano.enableDebugging();

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while (!softSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while (softSerial.available()) softSerial.read();

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

