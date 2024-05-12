/*
Date: 25-04-2024
Author: Folmer Heikamp
Board: Arduino Uno R4 Wifi
Requires:  UHF m6e nano shield

used for testing
*/


//Used for transmitting to the device
//If you run into compilation errors regarding this include, see the README
#include <SoftwareSerial.h>

SoftwareSerial softSerial(2, 3); //RX, TX

//#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
#include <mysparkfunlibrary.h>

RFID nano; //Create instance
uint16_t total = 0;
uint16_t success = 0;

void setup()
{
  Serial.begin(115200);

  while (!Serial);
  Serial.println();
  Serial.println("Initializing...");
  Serial.println(F("Select one of the following options:"));
  Serial.println(F(" (1) Read tags"));
  Serial.println(F(" (2) Show tag content"));
  Serial.println(F(" (3) Write new EPC to tag"));
  Serial.println(F(" (4) Write user data to tag"));
  Serial.println(F(" (5) Write passwords to tag"));
  Serial.println(F(" (6) Test filters"));
}

void printBytes(byte *bytes, uint8_t len)
{
  for (byte x = 0 ; x < len ; x++)
  {
    Serial.print(bytes[x], HEX);
    Serial.print(" ");
  }
  Serial.println("");
}

// Runs test filter with the same result
void testData(byte *data, uint8_t &len, byte *expectedData, uint8_t expectedLen)
{
  testFilter(0, data, len, 0, expectedData, expectedLen);
}

// Tests if the filter returned is correct
void testFilter(uint8_t result, byte *data, uint8_t &len, uint8_t expectedResult, byte *expectedData, uint8_t expectedLen) 
{
  // test result
  if(result != expectedResult)
  {
    Serial.println(F("ERROR!!! Result mismatch"));
    Serial.print(F("Real result: "));
    Serial.println(result, HEX);
    Serial.print(F("Expected result: "));
    Serial.println(expectedResult, HEX);
  }
  else
  {
    Serial.print(F("Result matches: "));
    Serial.println(result, HEX);

    if(len != expectedLen)
    {
      Serial.println(F("ERROR!!! Length mismatch"));
      Serial.print(F("Real length: "));
      Serial.println(len);
      Serial.print(F("Expected length: "));
      Serial.println(expectedLen);
    }
    else 
    {
      Serial.print(F("Length matches: "));
      Serial.println(len);
      byte x;
      for (x = 0 ; x < len ; x++)
      {
        if(data[x] != expectedData[x]) 
        {
          break;
        }
      }
      if(x != len)
      {
        Serial.print(F("ERROR!!! Data mismatch at byte "));
        Serial.println(x);
        Serial.print(F("Real data: "));
        printBytes(data, x + 1);
        Serial.print(F("Expected result: "));
        printBytes(expectedData, x + 1);
      }
      else
      {
        Serial.print(F("Data matches: "));
        printBytes(data, len);
        success++;
      }
    }
  }
  // increase total and reset length
  total++;
  len = 0;
}



void loop()
{
  Serial.println(F("Press key to start testing."));
  while (!Serial.available());
  int option = Serial.parseInt();

  // test filters
  byte data[255];
  ReadConfig config;
  TagFilter filter;
  uint8_t i = 0, result;

  // EPC TESTS
  byte epc[] = {0x98, 0x12, 0xAB};
  filter = nano.initEPCReadFilter(epc, sizeof(epc));

  // filter 1 - EPC standard
  byte expectedData1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x18, 0x98, 0x12, 0xAB, 0x04, 0x00, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x14, expectedData1, sizeof(expectedData1));
  // filter 1b - no metadata and no multitag
  filter.isMultiselect = false;
  filter.useMetadata = false;
  byte expectedData1b[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x18, 0x98, 0x12, 0xAB};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x04, expectedData1b, sizeof(expectedData1b));
  filter.isMultiselect = true;
  filter.useMetadata = true;

  // filter 2 - EPC with invert
  filter.isInverse = true;
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x1c, expectedData1, sizeof(expectedData1));
  filter.isInverse = false;

  // filter 3 - EPC with action 5 and target 3
  filter.target = 3;
  filter.action = 5;
  byte expectedData3[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x18, 0x98, 0x12, 0xAB, 0x03, 0x05, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x14, expectedData3, sizeof(expectedData3));

  byte longepc[] = {0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 
                    0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 
                    0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 
                    0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 
                    0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44}; // 320 bits
  filter = nano.initEPCReadFilter(longepc, sizeof(longepc));
  // filter 4 - EPC with extended length
  byte expectedData4[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x01, 0x40, 
                          0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
                          0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
                          0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
                          0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
                          0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44, 
                          0x04, 0x00, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x34, expectedData4, sizeof(expectedData4));

  // filter 5 - EPC with extended length and inverse
  filter.isInverse = true;
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x3C, expectedData4, sizeof(expectedData4));
  filter.isInverse = false;

  // USER FILTERS
  byte userdata[] = {0x11, 0x22, 0x33, 0x44};
  filter = nano.initUserDataReadFilter(userdata, sizeof(userdata));
  //filter 6 - Userdata from start 0x00 and action is 1
  Serial.println(F("Filter 6: USER start at 0x00"));
  i = 0;
  filter.action = 0x01;
  byte expectedData6[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x11, 0x22, 0x33, 0x44, 0x04, 0x01, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x13, expectedData6, sizeof(expectedData6));

  //filter 7 - Userdata from start 0x00 and action is 1 with start address at 0x22 and inverse
  Serial.println(F("Filter 7: USER start at 0x22 with inverse"));
  i = 0;
  filter.isInverse = true;
  filter.start = 0x22;
  byte expectedData7[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x20, 0x11, 0x22, 0x33, 0x44, 0x04, 0x01, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x1B, expectedData7, sizeof(expectedData7));
  filter.isInverse = false;

  //filter 8 - Userdata with extended filter
  byte longuserdata[] = {0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB,
                        0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB,
                        0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB,
                        0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB}; // 48 * 8 = 384 bits
  filter = nano.initUserDataReadFilter(longuserdata, sizeof(longuserdata));
  Serial.println(F("Filter 8: large user filter"));
  byte expectedData8[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80,
                          0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB,
                          0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB,
                          0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB,
                          0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 0xFE, 0x11, 0x22, 0x33, 0x44, 0xAB, 
                          0x04, 0x00, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x33, expectedData8, sizeof(expectedData8));

  // EPC LENGTH
  uint16_t length = 112;
  filter = nano.initEPCLengthReadFilter(length);
  //filter 9 - length 96
  Serial.println(F("Filter 9: EPC length"));
  byte expectedData9[] = {0x00, 0x70, 0x04, 0x00, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x16, expectedData9, sizeof(expectedData9));

  //filter 10 - length 96 with inverse
  Serial.println(F("Filter 10: EPC length with inverse"));
  filter.isInverse = true;
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x16, expectedData9, sizeof(expectedData9));
  filter.isInverse = false;

  //TID FILTERS
  //filter 11 - TID 0x8812AB start from 0x10 with action 3
  byte tid[] = {0x88, 0x12, 0xAB};
  filter = nano.initTIDReadFilter(tid, sizeof(tid));
  Serial.println(F("Filter 11: TID"));
  byte expectedData11[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x18, 0x88, 0x12, 0xAB, 0x04, 0x03, 0x00};
  filter.action = 0x03;
  filter.start = 0x10;
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x12, expectedData11, sizeof(expectedData11));

  //filter 12 - TID with inverse  
  Serial.println(F("Filter 12: TID with invert"));
  filter.isInverse = true;
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x1A, expectedData11, sizeof(expectedData11));
  filter.isInverse = false;

  // Password FILTERS
  //filter 13 - password
  uint32_t password = 0x12345678;
  filter = nano.initPasswordFilter(password);
  Serial.println(F("Filter 13: password"));
  byte expectedData13[] = {0x12, 0x34, 0x56, 0x78, 0x04, 0x00, 0x00};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x05, expectedData13, sizeof(expectedData13));

  // filter 14 - test inverse and multiselect and metadata
  Serial.println(F("Filter 13: password without multiselect"));
  filter.isMultiselect = false;
  filter.isInverse = true;
  filter.useMetadata = false;
  byte expectedData14[] = {0x12, 0x34, 0x56, 0x78};
  result = nano.constructFilterMsg(data, i, filter);
  testFilter(result, data, i, 0x05, expectedData14, sizeof(expectedData14));

  // TEST READ FUNCTIONS
  // TEST 0x22
  config = nano.initStandardReadMultipleTagsOnceConfig();

  // Read 1 (0x22) - without filter and simple config
  Serial.println(F("Read 1"));
  byte expected[] = {0x88, 0x10, 0x00, 0x13, 0x07, 0xD0, 0x00, 0x57};
  nano.constructReadTagIdMultipleMsg(data, i, config, filter);
  testData(data, i, expected, sizeof(expected));

  // Read 2 (0x22) - with EPC filter
  filter = nano.initEPCReadFilter(epc, sizeof(epc));
  Serial.println(F("Read 2"));
  byte expected2[] = {0x88, 0x14, 0x00, 0x13, 0x07, 0xD0, 0x00, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x18, 0x98, 0x12, 0xAB, 0x04, 0x00, 0x00};
  nano.constructReadTagIdMultipleMsg(data, i, config, filter);
  testData(data, i, expected2, sizeof(expected2));

  // Read 3 (0x22) - with EPC filter and different multiselect, searchflag, metadata and continuous enabled
  Serial.println(F("Read 3"));
  config.isContinuous = true;
  config.offtime = 0x1234;
  config.multiSelect = 0x85;
  config.searchFlag = 0x3333;
  config.metadataFlag = 0x1122;
  byte expected3[] = {0x85, 0x14, 0x33, 0x33, 0x07, 0xD0, 0x12, 0x34, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x18, 0x98, 0x12, 0xAB, 0x04, 0x00, 0x00};
  nano.constructReadTagIdMultipleMsg(data, i, config, filter);
  testData(data, i, expected3, sizeof(expected3));

  // Continuous Read 1 - standard
  config = nano.initStandardContinuousReadConfig();
  Serial.println(F("Continuous Read 1"));
  byte expectedCon1[] = {0x00, 0x00, 0x01, 0x22, 0x00, 0x00, 0x05, 0x1B, 0x22, 0x88, 0x14, 0x05, 0x1B, 0x03, 0xE8, 0x00, 0xFA, 0x00, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x18, 0x98, 0x12, 0xAB, 0x04, 0x00, 0x00};
  nano.constructMultiProtocolTagOpMsg(data, i, config, filter);
  testData(data, i, expectedCon1, sizeof(expectedCon1));

  // Read Data 1 - standard use filter
  config = nano.initStandardReadTagDataOnce();
  Serial.println(F("Read Data 1"));
  byte expectedData1[] = {0x07, 0xD0, 0x11, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x98, 0x12, 0xA}

  // Write EPC Tests
  // Write EPC 1 - no filter
  byte expectedWriteData = {0x07, 0xD0, 0x00, 0x00, 0x98, 0x12, 0xAB, 0x32, 0xFF, 0x00, 0x02, 0x06, 0x05, 0xE4, 0x01, 0xF6};

  // Write EPC 2 - filter 
  epc = {0xE2, 0x00, 0x42, 0x00, 0x8E, 0x80, 0x60, 0x17, 0x06, 0x8E, 0x05, 0xF0};
  filter = nano.initStandardWriteEPCFilter(epc, sizeof(epc));
  byte expectedWriteData = {0x07, 0xD0, 0x00, 0x00, 0x98, 0x12, 0xAB, 0x32, 0xFF, 0x00, 0x02, 0x06, 0x05, 0xE4, 0x01, 0xF6};

  // 

  // empty buffer
  while(Serial.available() > 0) Serial.read();
}
