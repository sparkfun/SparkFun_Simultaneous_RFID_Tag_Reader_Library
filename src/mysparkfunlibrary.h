/*
  Library for controlling the Nano M6E from ThingMagic
  This is a stripped down implementation of the Mercury API from ThingMagic

  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  License: Open Source MIT License
  If you use this code please consider buying an awesome board from SparkFun. It's a ton of
  work (and a ton of fun!) to put these libraries together and we want to keep making neat stuff!
  https://opensource.org/licenses/MIT

  The above copyright notice and this permission notice shall be included in all copies or 
  substantial portions of the Software.
*/

#include "response.h"

class RFID
{
public:
  RFID(void);

  void begin(Stream &serialPort = Serial); //If user doesn't specify then Serial will be used

  void enableDebugging(Stream &debugPort = Serial); //Turn on command sending and response printing. If user doesn't specify then Serial will be used
  void disableDebugging(void);

  void setBaud(long baudRate);
  void getVersion(void);
  void setReadPower(int16_t powerSetting);
  void getReadPower();
  void setWritePower(int16_t powerSetting);
  void getWritePower();
  void setRegion(uint8_t region);
  void setAntennaPort();
  void setAntennaSearchList();
  void setTagProtocol(uint8_t protocol = 0x05);

  void startReading();
  void startReadingWithFilterConfig(ReadConfig &readConfig, TagFilter &filter); 
  void stopReading(void);  //Stops continuous read. Give 1000 to 2000ms for the module to stop reading.

  void pinMode(uint8_t pin, uint8_t mode);
  void digitalWrite(uint8_t pin, uint8_t state);
  bool digitalRead(uint8_t pin);

  void enableReadFilter(void);
  void disableReadFilter(void);

  void setReaderConfiguration(uint8_t option1, uint8_t option2);
  void getOptionalParameters(uint8_t option1, uint8_t option2);
  void setProtocolParameters(void);
  void getProtocolParameters(uint8_t option1, uint8_t option2);


  bool check(void);
  
  //ReadConfig RFID::initReadConfig(uint16_t searchFlag, uint16_t metadataFlag, bool continuous = false, uint16_t offtime = 0, uint16_t streamStats = 0, bool readNTags = false, uint16_t N = 0);
  
  // simple filters
  TagFilter initEmptyFilter();
  TagFilter initEmptyFilterWithMetadata();
  TagFilter initEPCReadFilter(uint8_t *EPC, uint16_t EPCLength); 
  TagFilter initEPCSingleReadFilter(uint8_t *EPC, uint16_t EPCLength); 
  TagFilter initEPCLengthReadFilter(uint16_t EPCBitLength); 
  TagFilter initUserDataReadFilter(uint8_t *data, uint16_t dataLength); 
  TagFilter initTIDReadFilter(uint8_t *tid, uint16_t tidLength); 
  TagFilter initPasswordFilter(uint32_t password); 
  TagFilter initEPCWriteFilter(uint8_t *EPC, uint16_t EPCLength); 
  void setAccessPasswordFilter(uint32_t password);

  // simple configurations
  ReadConfig initStandardReadMultipleTagsOnceConfig();
  ReadConfig initStandardContinuousReadConfig();
  ReadConfig initStandardReadTagDataOnce();

  uint8_t constructFilterMsg(uint8_t *data, uint8_t &i, TagFilter &filter); 
  void constructMultiProtocolTagOpMsg(uint8_t *data, uint8_t &i, ReadConfig &readConfig, TagFilter &filter);
  void constructReadTagIdMultipleMsg(uint8_t *data, uint8_t &i, ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  void constructReadTagDataMsg(uint8_t *data, uint8_t &i, uint8_t bank, uint32_t address, ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  void constructWriteTagIdMsg(uint8_t *data, uint8_t &i, uint8_t *newEPC, uint8_t newEPCLength, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  void constructWriteTagDataMsg(uint8_t *data, uint8_t &i, uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  
  uint8_t readTagEPC(uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readMultipleTags(uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readMultipleTagsWithFilterConfig(ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeTagEPC(uint8_t *newID, uint8_t newIDLength, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeTagEPCWithFilter(uint8_t *newEPC, uint8_t newEPCLength, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readData(uint8_t bank, uint32_t address, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readDataWithFilterConfig(uint8_t bank, uint32_t address, ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeData(uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeDataWithFilter(uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readUserData(uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeUserData(uint8_t *userData, uint8_t userDataLength, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readKillPW(uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeKillPW(uint8_t *password, uint8_t passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readAccessPW(uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeAccessPW(uint8_t *password, uint8_t passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readTID(uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readUID(uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t killTag(uint8_t *password, uint8_t passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);

  void sendMessage(uint8_t opcode, uint8_t *data = 0, uint8_t size = 0, uint16_t timeOut = COMMAND_TIME_OUT, boolean waitForResponse = true);
  void sendCommand(uint16_t timeOut = COMMAND_TIME_OUT, boolean waitForResponse = true);

  void printMessageArray(void);

  //Variables

  //This is our universal msg array, used for all communication
  //Before sending a command to the module we will write our command and CRC into it
  //And before returning, response will be recorded into the msg array. Default is 255 bytes.
  uint8_t msg[MAX_MSG_SIZE];
  Response response;

private:
  Stream *_nanoSerial; //The generic connection to user's chosen serial hardware

  Stream *_debugSerial; //The stream to send debug messages to if enabled

  uint8_t _head = 0; //Tracks the length of the incoming message as we poll the software serial

  boolean _printDebug = false; //Flag to print the serial commands we are sending to the Serial port for debug
};

static void printBytes(uint8_t *bytes, uint8_t len)
{
  for (uint8_t x = 0 ; x < len ; x++)
  {
    if (bytes[x] < 16) {Serial.print("0");}
      Serial.print(bytes[x], HEX);
  } 
  Serial.println("");
} 
