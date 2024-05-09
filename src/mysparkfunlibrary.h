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

#include "Arduino.h" //Needed for Stream

#define MAX_MSG_SIZE 255

#define TMR_SR_OPCODE_VERSION 0x03
#define TMR_SR_OPCODE_SET_BAUD_RATE 0x06
#define TMR_SR_OPCODE_READ_TAG_ID_SINGLE 0x21
#define TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE 0x22
#define TMR_SR_OPCODE_WRITE_TAG_ID 0x23
#define TMR_SR_OPCODE_WRITE_TAG_DATA 0x24
#define TMR_SR_OPCODE_KILL_TAG 0x26
#define TMR_SR_OPCODE_READ_TAG_DATA 0x28
#define TMR_SR_OPCODE_GET_TAG_ID_BUFFER 0x29
#define TMR_SR_OPCODE_CLEAR_TAG_ID_BUFFER 0x2A
#define TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP 0x2F
#define TMR_SR_OPCODE_GET_READ_TX_POWER 0x62
#define TMR_SR_OPCODE_GET_WRITE_TX_POWER 0x64
#define TMR_SR_OPCODE_GET_USER_GPIO_INPUTS 0x66
#define TMR_SR_OPCODE_GET_POWER_MODE 0x68
#define TMR_SR_OPCODE_GET_READER_OPTIONAL_PARAMS 0x6A
#define TMR_SR_OPCODE_GET_PROTOCOL_PARAM 0x6B
#define TMR_SR_OPCODE_SET_ANTENNA_PORT 0x91
#define TMR_SR_OPCODE_SET_TAG_PROTOCOL 0x93
#define TMR_SR_OPCODE_SET_READ_TX_POWER 0x92
#define TMR_SR_OPCODE_SET_WRITE_TX_POWER 0x94
#define TMR_SR_OPCODE_SET_USER_GPIO_OUTPUTS 0x96
#define TMR_SR_OPCODE_SET_REGION 0x97
#define TMR_SR_OPCODE_SET_READER_OPTIONAL_PARAMS 0x9A
#define TMR_SR_OPCODE_SET_PROTOCOL_PARAM 0x9B

#define COMMAND_TIME_OUT 2000 //Number of ms before stop waiting for response from module

//Define all the ways functions can return
#define ALL_GOOD 0
#define ERROR_COMMAND_RESPONSE_TIMEOUT 1
#define ERROR_CORRUPT_RESPONSE 2
#define ERROR_WRONG_OPCODE_RESPONSE 3
#define ERROR_UNKNOWN_OPCODE 4
#define RESPONSE_IS_TEMPERATURE 5
#define RESPONSE_IS_KEEPALIVE 6
#define RESPONSE_IS_TEMPTHROTTLE 7
#define RESPONSE_IS_TAGFOUND 8
#define RESPONSE_IS_NOTAGFOUND 9
#define RESPONSE_IS_UNKNOWN 10
#define RESPONSE_SUCCESS 11
#define RESPONSE_FAIL 12

//Define the allowed regions - these set the internal freq of the module
#define REGION_INDIA 0x04
#define REGION_JAPAN 0x05
#define REGION_CHINA 0x06
#define REGION_EUROPE 0x08
#define REGION_KOREA 0x09
#define REGION_AUSTRALIA 0x0B
#define REGION_NEWZEALAND 0x0C
#define REGION_NORTHAMERICA 0x0D
#define REGION_OPEN 0xFF

//Taken from serial_reader_imp.h : TMR_SR_Gen2SingulationOptions enum
#define TMR_SR_GEN2_SINGULATION_OPTION_SELECT_DISABLED 0x00
#define TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_EPC 0x01
#define TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_TID 0x02
#define TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_USER_MEM 0x03
#define TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_ADDRESSED_EPC 0x04
#define TMR_SR_GEN2_SINGULATION_OPTION_USE_PASSWORD 0x05
#define TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_LENGTH_OF_EPC 0x06
#define TMR_SR_GEN2_SINGULATION_OPTION_SELECT_GEN2TRUNCATE 0x07
#define TMR_SR_GEN2_SINGULATION_OPTION_INVERSE_SELECT_BIT 0x08
#define TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA 0x10
#define TMR_SR_GEN2_SINGULATION_OPTION_EXTENDED_DATA_LENGTH 0x20
#define TMR_SR_GEN2_SINGULATION_OPTION_SECURE_READ_DATA 0x40

//Taken from tmr_gen2.h : TMR_GEN2_Bank enum
#define TMR_GEN2_BANK_RESERVED 0x0
#define TMR_GEN2_BANK_EPC 0x1
#define TMR_GEN2_BANK_TID 0x2
#define TMR_GEN2_BANK_USER 0x3
#define TMR_GEN2_BANK_RESERVED_ENABLED 0x4
#define TMR_GEN2_EPC_LENGTH_FILTER 0x6
#define TMR_GEN2_EPC_TRUNCATE 0x7
#define TMR_GEN2_BANK_EPC_ENABLED 0x8
#define TMR_GEN2_BANK_EPC_NO_PROTOCOL 0x09 // added bank --> for generic filters (non GEN2)
#define TMR_GEN2_BANK_TID_ENABLED 0x10
#define TMR_GEN2_BANK_USER_ENABLED 0x20

// define a TagFilter to make code a bit more readable
typedef struct TagFilter
{
  uint8_t type = TMR_SR_GEN2_SINGULATION_OPTION_SELECT_DISABLED;
  uint32_t password = 0x00;
  uint32_t start = 0x00;
  uint16_t filterDataBitLength = 0x00; // note that if second byte is used "EXTENDED_DATA_LENGTH" needs to be set
  uint8_t *filterData = NULL;
  uint8_t target = 0x04;
  uint8_t action = 0x00;
  bool isInverse = false;
  bool isMultiselect = false;
  bool isSecure = false;
  bool useMetadata = false;
};

// define a ReadConfig to make code more readable
typedef struct ReadConfig 
{
  uint8_t multiSelect = 0x00; // can we select multiple tags (0x88) -->
  uint16_t searchFlag = 0x00; // what kind of search flag -->
  uint16_t metadataFlag = 0x00; // what metadata do we collect --> 
  bool isContinuous = false; // are we in continuous mode -->
  uint16_t offtime = 0x00; // offtime -->
  uint16_t streamStats = 0x00;
  bool readNTags = false; // are we in Ntags mode -->
  uint16_t N = 0x00;
};

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
  void startReadingWithFilterConfig(uint16_t offtime, ReadConfig &readConfig, TagFilter &filter); 
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

  uint8_t parseResponse(void);

  uint8_t getTagEPCBytes(void);   //Pull number of EPC data bytes from record response.
  uint8_t getTagDataBytes(void);  //Pull number of tag data bytes from record response. Often zero.
  uint16_t getTagTimestamp(void); //Pull timestamp value from full record response
  uint32_t getTagFreq(void);      //Pull Freq value from full record response
  int8_t getTagRSSI(void);        //Pull RSSI value from full record response

  bool check(void);
  uint8_t checkResponse(uint8_t *dataRead, uint8_t &dataLengthRead);
  
  //ReadConfig RFID::initReadConfig(uint16_t searchFlag, uint16_t metadataFlag, bool continuous = false, uint16_t offtime = 0, uint16_t streamStats = 0, bool readNTags = false, uint16_t N = 0);
  
  // simple filters
  TagFilter initEmptyFilter();
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
  void constructReadTagDataMsg(uint8_t *data, uint8_t &i, uint8_t bank, uint32_t address, uint8_t dataLengthRead, ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  void constructWriteTagIdMsg(uint8_t *data, uint8_t &i, uint8_t *newEPC, uint8_t newEPCLength, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  void constructWriteTagDataMsg(uint8_t *data, uint8_t &i, uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  
  uint8_t readTagEPC(uint8_t *epc, uint8_t &epcLength, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readMultipleTags(uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readMultipleTagsWithFilterConfig(ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeTagEPC(uint8_t *newID, uint8_t newIDLength, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeTagEPCWithFilter(uint8_t *newEPC, uint8_t newEPCLength, TagFilter &filter,  uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readData(uint8_t bank, uint32_t address, uint8_t *dataRead, uint8_t &dataLengthRead, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readDataWithFilterConfig(uint8_t bank, uint32_t address, uint8_t *dataRead, uint8_t &dataLengthRead,  ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeData(uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeDataWithFilter(uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, TagFilter &filter, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readUserData(uint8_t *userData, uint8_t &userDataLength, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeUserData(uint8_t *userData, uint8_t userDataLength, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readKillPW(uint8_t *password, uint8_t &passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeKillPW(uint8_t *password, uint8_t passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readAccessPW(uint8_t *password, uint8_t &passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t writeAccessPW(uint8_t *password, uint8_t passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t readTID(uint8_t *tid, uint8_t &tidLength, uint16_t timeOut = COMMAND_TIME_OUT);
  uint8_t readUID(uint8_t *tid, uint8_t &tidLength, uint16_t timeOut = COMMAND_TIME_OUT);

  uint8_t killTag(uint8_t *password, uint8_t passwordLength, uint16_t timeOut = COMMAND_TIME_OUT);

  void sendMessage(uint8_t opcode, uint8_t *data = 0, uint8_t size = 0, uint16_t timeOut = COMMAND_TIME_OUT, boolean waitForResponse = true);
  void sendCommand(uint16_t timeOut = COMMAND_TIME_OUT, boolean waitForResponse = true);

  void printMessageArray(void);

  uint16_t calculateCRC(uint8_t *u8Buf, uint8_t len);

  //Variables

  //This is our universal msg array, used for all communication
  //Before sending a command to the module we will write our command and CRC into it
  //And before returning, response will be recorded into the msg array. Default is 255 bytes.
  uint8_t msg[MAX_MSG_SIZE];

  //uint16_t tags[MAX_NUMBER_OF_TAGS][12]; //Assumes EPC won't be longer than 12 bytes
  //uint16_t tagRSSI[MAX_NUMBER_OF_TAGS];
  //uint16_t uniqueTags = 0;

private:
  Stream *_nanoSerial; //The generic connection to user's chosen serial hardware

  Stream *_debugSerial; //The stream to send debug messages to if enabled

  uint8_t _head = 0; //Tracks the length of the incoming message as we poll the software serial

  boolean _printDebug = false; //Flag to print the serial commands we are sending to the Serial port for debug
};
