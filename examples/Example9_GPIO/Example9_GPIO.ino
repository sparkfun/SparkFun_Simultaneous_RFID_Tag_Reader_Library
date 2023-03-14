/*
  Read and write GPIO pins of the M6E Nano
  By: Dryw Wade @ SparkFun Electronics
  Date: October 24th, 2022
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Reads the state of GPIO pin 1, and toggles the state of GPIO pin 2

  If using the Simultaneous RFID Tag Reader (SRTR) shield, make sure the serial slide
  switch is in the 'SW-UART' position
*/

//Used for transmitting to the device
//If you run into compilation errors regarding this include, see the README
#include <SoftwareSerial.h>

SoftwareSerial softSerial(2, 3); //RX, TX

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //Create instance

// These are the pins on the M6E module, which can be 1, 2, 3, or 4
uint8_t inputPin = 1;
uint8_t outputPin = 2;

void setup()
{
  Serial.begin(115200);
  while (!Serial); //Wait for the serial port to come online

  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  Serial.println(F("Module connected!"));

  // Set each pin to the appropriate state
  nano.pinMode(inputPin, INPUT);
  nano.pinMode(outputPin, OUTPUT);
}

void loop()
{
  // Read the current state of the input pin
  Serial.println(nano.digitalRead(inputPin));

  // Toggle the output pin
  nano.digitalWrite(outputPin, HIGH);
  delay(500);
  nano.digitalWrite(outputPin, LOW);
  delay(500);
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while (softSerial.isListening() == false); //Wait for port to open

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
