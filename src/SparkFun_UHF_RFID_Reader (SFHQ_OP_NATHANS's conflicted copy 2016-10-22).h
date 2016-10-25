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

#define TMR_SR_OPCODE_VERSION 0x03
#define TMR_SR_OPCODE_SET_BAUD_RATE 0x06
#define TMR_SR_OPCODE_READ_TAG_ID_SINGLE 0x21
#define TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE 0x22
#define TMR_SR_OPCODE_WRITE_TAG_ID 0x23
#define TMR_SR_OPCODE_WRITE_TAG_DATA 0x24
#define TMR_SR_OPCODE_KILL_TAG 0x26
#define TMR_SR_OPCODE_READ_TAG_DATA 0x28
#define TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP 0x2F
#define TMR_SR_OPCODE_GET_READ_TX_POWER 0x62
#define TMR_SR_OPCODE_GET_POWER_MODE 0x68
#define TMR_SR_OPCODE_GET_READER_OPTIONAL_PARAMS 0x6A
#define TMR_SR_OPCODE_GET_PROTOCOL_PARAM 0x6B
#define TMR_SR_OPCODE_SET_ANTENNA_PORT 0x91
#define TMR_SR_OPCODE_SET_TAG_PROTOCOL 0x93
#define TMR_SR_OPCODE_SET_READ_TX_POWER 0x92
#define TMR_SR_OPCODE_SET_WRITE_TX_POWER 0x94
#define TMR_SR_OPCODE_SET_REGION 0x97
#define TMR_SR_OPCODE_SET_READER_OPTIONAL_PARAMS 0x9A
#define TMR_SR_OPCODE_SET_PROTOCOL_PARAM 0x9B

#define COMMAND_TIME_OUT  2000 //Number of ms before stop waiting for response from module

//Define all the ways functions can return
#define ALL_GOOD                        0
#define ERROR_COMMAND_RESPONSE_TIMEOUT  1
#define ERROR_CORRUPT_RESPONSE          2
#define ERROR_WRONG_OPCODE_RESPONSE     3
#define ERROR_UNKNOWN_OPCODE            4
#define RESPONSE_IS_TEMPERATURE         5
#define RESPONSE_IS_KEEPALIVE           6
#define RESPONSE_IS_TEMPTHROTTLE        7
#define RESPONSE_IS_TAGFOUND            8
#define RESPONSE_IS_UNKNOWN             9

//Create global database of tags
#define MAX_NUMBER_OF_TAGS              10 //10 is good. Careful, you can run out of memory quickly.

class RFID
{
  public:
    RFID(void);

    bool begin(uint8_t *msg, Stream &serialPort = Serial); //If user doesn't specificy then Serial will be used

    void setBaud(long baudRate);
    void getVersion(void);
    void setReadPower(int16_t powerSetting);
    void getReadPower();
    void setWritePower(int16_t powerSetting);
    void setRegion(uint8_t region);
    void setAntennaPort();
    void setAntennaSearchList();
	void setTagProtocol(uint16_t protocol = 0x05);

    void startReading(void); //Disable filtering and start reading continuously
    void stopReading(void); //Stops continuous read. Give 1000 to 2000ms for the module to stop reading.

    void enableReadFilter(void);
    void disableReadFilter(void);

    void setReaderConfiguration(void);
    void getOptionalParameters(uint8_t option1, uint8_t option2);
    void setProtocolParameters(void);
    void getProtocolParameters(uint8_t option1, uint8_t option2);

    uint8_t parseResponse(void);
    
    int8_t getRSSI(void); //Pull RSSI value from full record response
    
    bool check(void);

    void writeID(uint8_t *newID, uint8_t newIDLength, uint16_t timeOut = COMMAND_TIME_OUT);

    void writeUserData(uint8_t *userData, uint8_t userDataLength, uint16_t timeOut = COMMAND_TIME_OUT);
    void readUserData(uint8_t *epc, uint16_t epcLength, uint16_t timeOut = COMMAND_TIME_OUT);

    void killTag(uint32_t pw, uint16_t timeOut = COMMAND_TIME_OUT);

    void sendMessage(uint8_t opcode, uint8_t *data = 0, uint8_t size = 0, boolean waitForResponse = true);
    void sendCommand(boolean waitForResponse = true);

    uint16_t calculateCRC(uint8_t *u8Buf, uint8_t len);

    uint16_t tags[MAX_NUMBER_OF_TAGS][12]; //Assumes EPC won't be longer than 12 bytes
    uint16_t tagRSSI[MAX_NUMBER_OF_TAGS];  
    uint16_t uniqueTags = 0;

  private:

    Stream *_nanoSerial; //The generic connection to user's chosen serial hardware
    uint8_t *_nanoMsg; //This is the global array that is used for each serial communication

    uint8_t _head = 0; //Tracks the length of the incoming message as we poll the software serial
};
