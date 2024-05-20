#include "Arduino.h" //Needed for Stream

#define MAX_MSG_SIZE 255

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

//Metadata flags, taken from tmr_tag_data.h   
#define TMR_TRD_METADATA_FLAG_NONE 0x0000
#define TMR_TRD_METADATA_FLAG_READCOUNT 0x0001
#define TMR_TRD_METADATA_FLAG_RSSI 0x0002
#define TMR_TRD_METADATA_FLAG_ANTENNAID 0x0004
#define TMR_TRD_METADATA_FLAG_FREQUENCY 0x0008
#define TMR_TRD_METADATA_FLAG_TIMESTAMP 0x0010
#define TMR_TRD_METADATA_FLAG_PHASE 0x0020
#define TMR_TRD_METADATA_FLAG_PROTOCOL 0x0040
#define TMR_TRD_METADATA_FLAG_DATA 0x0080
#define TMR_TRD_METADATA_FLAG_GPIO_STATUS 0x0100
#define TMR_TRD_METADATA_FLAG_GEN2_Q 0x0200
#define TMR_TRD_METADATA_FLAG_GEN2_LF 0x0400
#define TMR_TRD_METADATA_FLAG_GEN2_TARGET 0x0800
#define TMR_TRD_METADATA_FLAG_BRAND_IDENTIFIER 0x1000
#define TMR_TRD_METADATA_FLAG_TAGTYPE 0x2000
#define TMR_TRD_METADATA_FLAG_MAX 0x2000

// Metadata must match tmr_tag_data.h! 
enum MetadataId
{
  NONE,
  READCOUNT,
  RSSI,
  ANTENNAID,
  FREQUENCY,
  TIMESTAMP,
  PHASE,
  PROTOCOL,
  DATA,
  GPIO_STATUS,
  GEN2_Q,
  GEN2_LF,
  GEN2_TARGET,
  BRAND_IDENTIFIER,
  TAGTYPE,
  TOTAL_METADATA,
};

// labels for printing, make sure they match with MetadataId and metadataLengths
static char *metadataLabels[] = {"", "Readcount", "RSSI", "Antenna ID", "Frequency", 
                                "Timestamp", "Phase", "Protocol", "Embedded Data Length", "GPIO Status",
                                "Gen2 Q", "Gen2 LF", "Gen2 Target", "Brand Identifier", "Tag Type"};

// only for the elements with a fixed size (embedded data and tag type are dynamic)
static uint8_t metadataLengths[TOTAL_METADATA] = {0, 1, 1, 1, 3,
                                                  4, 2, 1, 2, 1, 
                                                  1, 1, 1, 2, 0};

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
#define ALL_GOOD 0x0000
#define ERROR_COMMAND_RESPONSE_TIMEOUT 1
#define ERROR_CORRUPT_RESPONSE 2
#define ERROR_WRONG_OPCODE_RESPONSE 3
#define ERROR_UNKNOWN_OPCODE 4
#define RESPONSE_IS_TEMPERATURE 5
#define RESPONSE_IS_KEEPALIVE 0x0400
#define RESPONSE_IS_TEMPTHROTTLE 0x0504
#define RESPONSE_IS_HIGHRETURNLOSS 0x0505
#define RESPONSE_IS_TAGFOUND 8
#define RESPONSE_IS_NOTAGFOUND 9
#define RESPONSE_IS_UNKNOWN 10
#define RESPONSE_SUCCESS 11
#define RESPONSE_FAIL 12


// define a TagFilter to make code a bit more readable (size: 18 bytes)
typedef struct TagFilter
{
  uint8_t type = TMR_SR_GEN2_SINGULATION_OPTION_SELECT_DISABLED;
  uint32_t password = 0x00;
  uint32_t start = 0x00;
  uint16_t filterDataBitLength = 0x00; // note that if second byte is used "EXTENDED_DATA_LENGTH" needs to be set
  uint8_t *filterData = NULL; // use arrays to prevent memory issues
  uint8_t target = 0x04;
  uint8_t action = 0x00;
  bool isInverse = false;
  bool isMultiselect = false;
  bool isSecure = false;
};

// define a ReadConfig to make code more readable (size: 13 bytes)
typedef struct ReadConfig 
{
  uint8_t multiSelect = 0x00; // can we select multiple tags (0x88) 
  uint16_t searchFlag = 0x00; // what kind of search flag 
  uint16_t metadataFlag = 0x00; // what metadata do we collect 
  bool isContinuous = false; // are we in continuous mode 
  uint16_t offtime = 0x00; // offtime
  uint16_t streamStats = 0x00;
  bool readNTags = false; // are we in Ntags mode 
  uint16_t N = 0x00;
};

// This class stores the response of requests we are interested in
// Provides a few neat features at the cost of some memory (524 bytes of data):
// 1. stores data longer than msg (which is needed in case of a 0x29)
// 2. allows efficient searching
// 3. allows storage of more than 1 tags (overloads + operator)
// 4. can handle multiple metadata masks
// performance overhad should be minimal since only 1 response object is kept at all time
class Response
{
  public:
    uint16_t msgLength = 0; // length of message
    uint8_t msg[512]; // message buffer
    uint16_t status = 0x0000; //
    uint8_t opcode = 0; // opcode
    uint16_t metadataFlag = 0; // metadata flag
    uint8_t nrTags = 0; // the number of items contained in this repsonse
    uint8_t metadataOffsets[TOTAL_METADATA]; // store 15 offsets
    uint8_t metadataLength = 0;
    uint8_t headerLength = 0;
    uint8_t temperature = 0;

    // constructors
    Response();
    Response(uint8_t* msg, uint8_t msgLength);

    // overloaded + operator so we can add responses together
    Response& operator+(const Response &other);
    
    void reset();
    void parse(uint8_t* msg, uint8_t msgLength);
 
    uint16_t getTagPointer(uint8_t tag, uint16_t &embeddedLength, uint8_t &tagTypeLength);
    void getData(uint8_t tag, uint8_t *buf, uint16_t &bufLength, uint8_t start);
    void getBankdata(uint8_t item, uint8_t *buf, uint16_t &bufLength);
    void getEPCdata(uint8_t tag, uint8_t *buf, uint16_t &bufLength);
    void getMetadata(uint8_t tag, uint8_t *buf, uint16_t &bufLength);
    uint16_t metadataToJsonString(uint8_t tag, char *buf, int bufLength);

  private:
    void calculateMetadataOffsets();


};

//Comes from serial_reader_l3.c
//ThingMagic-mutated CRC used for messages.
//Notably, not a CCITT CRC-16, though it looks close.
static uint16_t crctable[] =
    {
        0x0000,
        0x1021,
        0x2042,
        0x3063,
        0x4084,
        0x50a5,
        0x60c6,
        0x70e7,
        0x8108,
        0x9129,
        0xa14a,
        0xb16b,
        0xc18c,
        0xd1ad,
        0xe1ce,
        0xf1ef,
};


//Calculates the magical CRC value
static uint16_t calculateCRC(uint8_t *u8Buf, uint8_t len)
{
  uint16_t crc = 0xFFFF;

  for (uint8_t i = 0; i < len; i++)
  {
    crc = ((crc << 4) | (u8Buf[i] >> 4)) ^ crctable[crc >> 12];
    crc = ((crc << 4) | (u8Buf[i] & 0x0F)) ^ crctable[crc >> 12];
  }

  return crc;
}

// converts an array of bytes to a hex string, not exceeding charsLength
// using new or malloc is possible, but arduino does not really like dynamic memory
// returns the numbers of characters written
static uint8_t bytesToHexString(uint8_t *bytes, uint8_t len, char *chars, int charsLength)
{
  uint8_t length = 0;
  for (uint8_t x = 0 ; x < len ; x++)
  {
    length += snprintf(chars + length, charsLength, "%02x", bytes[x]);
  } 
  length += snprintf(chars + length, charsLength, "\0");
  return length;
}

// prints a byte array
static void printBytes(uint8_t *bytes, uint8_t len)
{
  for (uint8_t x = 0 ; x < len ; x++)
  {
    if (bytes[x] < 16) {Serial.print("0");}
      Serial.print(bytes[x], HEX);
  } 
  Serial.println("");
} 
