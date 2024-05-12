/*
Date: 25-04-2024
Author: Folmer Heikamp
Board: Arduino Uno R4 Wifi
Requires:  UHF m6e nano shield

The GENERIC UHF RFID READER AND WRITER is an RFID reader and writer for UHF Gen2 tags.
It uses the SparkFun Simultaneous RFID Tag Reader Library, modified with support for filters.
It tries to mimick the functionalities offered by the Universal Reader Assistant.
Currently, the GENERIC UHF RFID READER AND WRITER offers the following functionality:
1. Reading a single random tag
2. Reading multiple tags with or withour a EPC filter
3. Inspecting the tag content for a given EPC
4. Writing to tag (EPC and Data) with or without EPC filter
Note that not all capabilities of the MercuryAPI are implemented in the SparkFun library.
So, functionality like locking a tag requires changes to the library.
*/

#include <mysparkfunlibrary.h>

RFID nano;

void setup()
{
  Serial.begin(115200);

  while (!Serial);
  Serial.println();
  Serial.println("Initializing...");

  if (setupNano(115200) == false) //Configure nano to run at 38400bps
  {
    Serial.println("Module failed to respond. Please check wiring.");
    while (1); //Freeze!
  }
  Serial.println(F("----------------------------------------"));
  Serial.println(F("|  GENERIC UHF READER AND WRITER V1.0  |"));
  Serial.println(F("----------------------------------------"));
  Serial.println(F("Select one of the following options:"));
  Serial.println(F(" (1) Read Random Tag EPC"));
  Serial.println(F(" (2) Read Tag EPCs"));
  Serial.println(F(" (3) Continuous read for 5 seconds"));
  Serial.println(F(" (4) Inspect Tag"));
  Serial.println(F(" (5) Write New Tag EPC"));
  Serial.println(F(" (6) Write New Tag Data"));

  nano.setRegion(REGION_AUSTRALIA); //Set to North America

  nano.setReadPower(1500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  nano.setWritePower(1500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Write TX Power is 27.00 dBm and may cause temperature-limit throttling
}


// ------------------------------------------------------------------
// HELPER FUNCTIONS (could be moved to library)
// ------------------------------------------------------------------

// Tests if the string is a valid EPC.
// Valid EPCs are in hex format and should be byte aligned
bool isEPC(String EPC) {
  // if uneven, it is not an EPC
  if(EPC.length() % 4 != 0) {
    return false;
  }
  // test if string is hexadecimal
  for(int i=0; i < EPC.length(); i++) {
    if(!isHexadecimalDigit(EPC[i])) {
      return false;
    }
  }
  return true;
}

// For converting a hex string into a byte array
void hexStringToByteArray(String s, byte *arr) {
  int length = s.length() / 2;
  for(int i=0; i < length * 2; i += 2) {
    // include 0-byte
    char tmp[3];
    s.substring(i, i + 2).toCharArray(tmp, 3);
    arr[i/2] = (byte) strtoul(tmp, NULL, 16);
  } 
}

// Gets an EPC from the user
String getUserEPC() {
  String EPC;
  do {
    Serial.println(F("Enter a valid EPC: "));
    while (!Serial.available());
    EPC = Serial.readString();
    EPC.trim();
  } while(!isEPC(EPC) && EPC != "");  
  return EPC;
}

// ------------------------------------------------------------------
// CLI FUNCTIONS
// ------------------------------------------------------------------
 
// Scans a single random tag using the readTagEPC function
void readRandomTagId() {
  uint16_t myEPClength = 12;
  byte myEPC[myEPClength]; 
  while (nano.response.status != RESPONSE_SUCCESS)
  {
    myEPClength = sizeof(myEPC);
    nano.readTagEPC(1500); 
    Serial.println(F("Searching for tag"));
  }
  if(nano.response.nrTags > 0)
  {
    nano.response.getData(0, myEPC, myEPClength, 4);
    Serial.print(F("EPC: "));
    printBytes(myEPC, myEPClength);
  }
}

// Scans for all tags once
// Supports simple EPC filters
void readMultipleTagIds() {
  Serial.println(F("Provide EPC Filter (\"\" to leave empty):"));
  String EPCFilter = getUserEPC();
  uint16_t EPCLength = 12;
  byte EPC[EPCLength];
  // apply an EPC filter
  if(EPCFilter != "") {
    uint8_t EPCFilterLength = EPCFilter.length() / 2;
    byte EPCFilterBytes[EPCFilterLength];
    hexStringToByteArray(EPCFilter, EPCFilterBytes);
    ReadConfig config = nano.initStandardReadMultipleTagsOnceConfig();
    TagFilter filter = nano.initEPCReadFilter(EPCFilterBytes, EPCFilterLength);
    nano.readMultipleTagsWithFilterConfig(config, filter);
  }
  else {
    nano.readMultipleTags();
  }
  Serial.print("Found: ");
  Serial.print(nano.response.nrTags);
  Serial.println(" tags!");
  for(uint8_t i = 0; i < nano.response.nrTags; i++) {
    nano.response.getEPCdata(i, EPC, EPCLength);
    Serial.print(F("EPC: "));
    printBytes(EPC, EPCLength);
    nano.response.printMetadata(i);
    Serial.println("");
  }
}

void pollTags(uint8_t time) {
  TagFilter filter;
  ReadConfig readConfig = nano.initStandardContinuousReadConfig();
  uint16_t EPCLength = 12;
  byte EPC[EPCLength];
  nano.startReadingWithFilterConfig(readConfig, filter);
  unsigned long end;
  unsigned long start = millis();
  do {
    if (nano.check() == true) //Check to see if any new data has come in from module
    {
      if (nano.response.status == RESPONSE_IS_KEEPALIVE)
      {
        Serial.println(F("Scanning"));
      }
      else if (nano.response.status == RESPONSE_IS_TAGFOUND && nano.response.nrTags > 0)
      {
        nano.response.getEPCdata(0, EPC, EPCLength);
        Serial.print(F("EPC: "));
        printBytes(EPC, EPCLength);
        nano.response.printMetadata(0);
        Serial.println("");
      }
      else if (nano.response.status == ERROR_CORRUPT_RESPONSE)
      {
        Serial.println("Bad CRC");
      }
    }
    end = millis();
  } while((end - start) / 1000 < time); // there is an overflow issue in that case the loop will also terminate
  nano.stopReading();
}

// Inspects a tag, if no EPC is provided, a random tag is inspected
void inspectTag() {
  String EPCFilter = getUserEPC();
  TagFilter filter;
  ReadConfig config;
  // enable the filter
  uint8_t EPCFilterLength = EPCFilter.length() / 2;
  byte EPCFilterBytes[EPCFilterLength];
  if(EPCFilter != "") {
    hexStringToByteArray(EPCFilter, EPCFilterBytes);
    config = nano.initStandardReadTagDataOnce();
    filter = nano.initEPCSingleReadFilter(EPCFilterBytes, EPCFilterLength);
  } 
  uint16_t buflen = 64;
  uint16_t b1 = buflen, b2 = buflen, b3 = buflen, b4 = buflen;
  byte buffer[buflen];
  nano.readDataWithFilterConfig(0x00, 0x00, config, filter);
  if(nano.response.nrTags > 0)
  {
    Serial.print(F("Reserved Bank: "));
    nano.response.getBankdata(0, buffer, b1);
    printBytes(buffer, b1);
  }
  nano.readDataWithFilterConfig(0x01, 0x00, config, filter);
  if(nano.response.nrTags > 0)
  {
    Serial.print(F("EPC Bank: "));
    nano.response.getBankdata(0, buffer, b2);
    printBytes(buffer, b2);
  }
  nano.readDataWithFilterConfig(0x02, 0x00, config, filter);
  if(nano.response.nrTags > 0)
  {
    Serial.print(F("TID Bank: "));
    nano.response.getBankdata(0, buffer, b3);
    printBytes(buffer, b3);
  }
  nano.readDataWithFilterConfig(0x03, 0x00, config, filter);
  if(nano.response.nrTags > 0)
  {
    Serial.print(F("User Bank: "));
    nano.response.getBankdata(0, buffer, b4);
    printBytes(buffer, b4);
  }
}

// Takes care of writing a new EPC to the tag
// Handles the user input, conversion and writing
void writeTagEPC() {
  // gets user input and tests if it is hexadecimal
  String oldEPC = getUserEPC();
  String newEPC = getUserEPC();
  TagFilter filter;
  if(newEPC != "") {
    // with filter
    if(oldEPC != "") {
      Serial.println("Old Tag EPC: " + oldEPC);
    }
    Serial.println("New Tag EPC: " + newEPC);

    uint8_t oldEPCLength = oldEPC.length() / 2;
    uint8_t newEPCLength = newEPC.length() / 2;
    byte oldBytes[oldEPCLength];
    byte newBytes[newEPCLength];
    hexStringToByteArray(oldEPC, oldBytes);
    hexStringToByteArray(newEPC, newBytes);
    if(oldEPC != "") {
      printBytes(oldBytes, oldEPCLength);
    }
    printBytes(newBytes, newEPCLength);
    if(oldEPC != "") {  
      filter = nano.initEPCWriteFilter(oldBytes, oldEPCLength);
      nano.writeTagEPCWithFilter(newBytes, newEPCLength, filter);
    }
    else {  
      nano.writeTagEPC(newBytes, newEPCLength);
    }

    if (nano.response.status == RESPONSE_SUCCESS) {
      Serial.println("New EPC Written!");
    }
    else {
      Serial.println("Failed write");
    }
  }
  else {
    Serial.println(F("New EPC cannot be empty"));
  }
}

void writeTagData() {
  // get user input and convert to bytes
  Serial.println(F("Provide input(ASCII):"));
  String input;
  while (!Serial.available());
  input = Serial.readString();
  byte inputBytes[input.length() + 1]; 
  input.getBytes(inputBytes, input.length() + 1);

  // get EPC filter
  Serial.println(F("Provide EPC Filter (\"\" to leave empty):"));
  String EPCFilter = getUserEPC();

  // if there is a filter
  if(EPCFilter != "") {
    Serial.println("writing " + input + " to " + EPCFilter);
    // prepare filter
    uint8_t EPCFilterLength = EPCFilter.length() / 2;
    byte EPCFilterBytes[EPCFilterLength];
    hexStringToByteArray(EPCFilter, EPCFilterBytes);
    TagFilter filter = nano.initEPCWriteFilter(EPCFilterBytes, EPCFilterLength);
    nano.writeUserData(inputBytes, sizeof(inputBytes) - 1);
  }
  // if there is no filter
  else {
    Serial.println("writing " + input + " to random tag");
    nano.writeUserData(inputBytes, sizeof(inputBytes) - 1);
  }

  // check response
  if (nano.response.status == RESPONSE_SUCCESS) {
    Serial.println("New Data Written!");
  }
  else {
    Serial.println("Failed write");
  }
}

void loop()
{
  Serial.println(F("Select an option"));
  while (!Serial.available());
  int option = Serial.parseInt();
  // empty buffer
  while(Serial.available() > 0) Serial.read(); 
  switch(option) {
    case 1:
      readRandomTagId();
      break;
    case 2:
      readMultipleTagIds();
      break;
    case 3:
      pollTags(5);
      break;
    case 4:
      inspectTag();
      break;
    case 5:
      writeTagEPC();
      break;
    case 6:
      writeTagData();
      break;
    default:
      Serial.println(F("Invalid option"));
      break;
  }
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.enableDebugging(Serial); 
  nano.begin(Serial1); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  Serial1.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while (!Serial1); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while (Serial1.available()) Serial1.read();

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
    Serial1.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    Serial1.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  //Serial.println(nano.msg);
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}
