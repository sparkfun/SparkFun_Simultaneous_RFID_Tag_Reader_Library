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

  To learn more about how ThingMagic controls the module please look at the following SDK files:
    serial_reader_l3.c - Contains the bulk of the low-level routines
    serial_reader_imp.h - Contains the OpCodes
    tmr__status_8h.html - Contains the Status Word error codes

  Available Functions:
    setBaudRate
    setRegion
    setReadPower
    startReading (continuous read)
    stopReading
    readTagEPC
    writeTagEPC
    readTagData
    writeTagData
    killTag
    (nyw) lockTag
*/

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "mysparkfunlibrary.h"

RFID::RFID(void)
{
  // Constructor
}

//Initialize the Serial port
void RFID::begin(Stream &serialPort)
{
  _nanoSerial = &serialPort; //Grab which port the user wants us to use

  //_nanoSerial->begin(); //Stream has no .begin() so the user has to do a whateverSerial.begin(xxxx); from setup()
}

//Enable or disable the printing of sent/response HEX values.
//Use this in conjunction with 'Transport Logging' from the Universal Reader Assistant to see what they're doing that we're not
void RFID::enableDebugging(Stream &debugPort)
{
  _debugSerial = &debugPort; //Grab which port the user wants us to use for debugging

  _printDebug = true; //Should we print the commands we send? Good for debugging
}
void RFID::disableDebugging(void)
{
  _printDebug = false; //Turn off extra print statements
}

//Set baud rate
//Takes in a baud rate
//Returns response in the msg array
void RFID::setBaud(long baudRate)
{
  //Copy this setting into a temp data array
  uint8_t size = sizeof(baudRate);
  uint8_t data[size];
  for (uint8_t x = 0; x < size; x++)
    data[x] = (uint8_t)(baudRate >> (8 * (size - 1 - x)));

  sendMessage(TMR_SR_OPCODE_SET_BAUD_RATE, data, size, COMMAND_TIME_OUT, false);
}


//Builds a standard config and empty filter
void RFID::startReading()
{
  TagFilter filter;
  ReadConfig readConfig = initStandardContinuousReadConfig();
  startReadingWithFilterConfig(0, readConfig, filter);
}

//Used for continuous reading
//Note that only the subcommand is customisable
//Could not find a good reason to include parameters for the main command
void RFID::constructMultiProtocolTagOpMsg(uint8_t *data, uint8_t &i, ReadConfig &readConfig, TagFilter &filter)
{
  uint8_t subleni;
  data[i++] = 0x00; // 
  data[i++] = 0x00; // timeout(2)
  data[i++] = 0x01; // option byte (no metadata)
  data[i++] = TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE;
  data[i++] = 0x00;
  data[i++] = 0x00; // search flags(2)
  data[i++] = 0x05; // Gen2 protocol
  subleni = i;
  data[i++] = 0x00; // subcommand length
  data[i++] = TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE; // subcommand length
  constructReadTagIdMultipleMsg(data, i, readConfig, filter, 0x03E8); // hardcoded timeout
  data[subleni] = i - subleni - 2; // calculate length of subcommand
}

//Begin scanning for tags
//There are many many options and features to the nano, this sets options
//for continuous read of GEN2 type tags
void RFID::startReadingWithFilterConfig(uint16_t offtime, ReadConfig &readConfig, TagFilter &filter)
{
  //Constructing message should not take too much resources, since it is only done once
  uint8_t i = 0;
  uint8_t data[MAX_MSG_SIZE];
  constructMultiProtocolTagOpMsg(data, i, readConfig, filter);

  sendMessage(TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP, data, i);
}

//Stop a continuous read
void RFID::stopReading()
{
  //00 00 = Timeout, currently ignored
  //02 = Option - stop continuous reading
  uint8_t configBlob[] = {0x00, 0x00, 0x02};

  sendMessage(TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP, configBlob, sizeof(configBlob), false); //Do not wait for response
}

// Set one of the GPIO pins as INPUT or OUTPUT
void RFID::pinMode(uint8_t pin, uint8_t mode)
{
  // {option flag, pin number, pin mode, pin state}
  uint8_t data[] = {1, pin, mode, 0};
  sendMessage(TMR_SR_OPCODE_SET_USER_GPIO_OUTPUTS, data, sizeof(data), COMMAND_TIME_OUT, false);
}

// For a pin configured as an OUTPUT, this sets that pin state HIGH or LOW
void RFID::digitalWrite(uint8_t pin, uint8_t state)
{
  // {pin number, pin state}
  uint8_t data[] = {pin, state};
  sendMessage(TMR_SR_OPCODE_SET_USER_GPIO_OUTPUTS, data, sizeof(data), COMMAND_TIME_OUT, false);
}

// For a pin configured as an INPUT, this returns that pin's state (HIGH/LOW)
bool RFID::digitalRead(uint8_t pin)
{
  // Send command to get current GPIO inputs, and wait for response
  uint8_t data[] = {1};
  sendMessage(TMR_SR_OPCODE_GET_USER_GPIO_INPUTS, data, sizeof(data), COMMAND_TIME_OUT, true);
  
  // Got response, parse the returned message
  uint8_t len = msg[1] - 1; // Number of bytes in message after offset
  uint8_t offset = 6; // Relevant data is offset by 6 bytes

  // Data is stored in sets of 3 bytes for each pin, where the first byte is the
  // pin number, second is the pin mode, and third is the pin state
  for(int i = 0; i < len; i += 3)
  {
    // Check if this is the requested pin
    if(msg[i + offset] == pin)
    {
      // Return this pin's state
      return msg[i + offset + 2];
    }
  }

  // Requested pin was not in message
  return false;
}


//Given a region, set the correct freq
//0x04 = IN
//0x05 = JP
//0x06 = PRC
//0x08 = EU3
//0x09 = KR2
//0x0B = AU
//0x0C = NZ
//0x0D = NAS2 (North America)
//0xFF = OPEN
void RFID::setRegion(uint8_t region)
{
  sendMessage(TMR_SR_OPCODE_SET_REGION, &region, sizeof(region));
}

//Sets the TX and RX antenna ports to 01
//Because the Nano module has only one antenna port, it is not user configurable
void RFID::setAntennaPort(void)
{
  uint8_t configBlob[] = {0x01, 0x01}; //TX port = 1, RX port = 1
  sendMessage(TMR_SR_OPCODE_SET_ANTENNA_PORT, configBlob, sizeof(configBlob));
}

//This was found in the logs. It seems to be very close to setAntennaPort
//Search serial_reader_l3.c for cmdSetAntennaSearchList for more info
void RFID::setAntennaSearchList(void)
{
  uint8_t configBlob[] = {0x02, 0x01, 0x01}; //logical antenna list option, TX port = 1, RX port = 1
  sendMessage(TMR_SR_OPCODE_SET_ANTENNA_PORT, configBlob, sizeof(configBlob));
}

//Sets the protocol of the module
//Currently only GEN2 has been tested and supported but others are listed here for reference
//and possible future support
//TMR_TAG_PROTOCOL_NONE              = 0x00
//TMR_TAG_PROTOCOL_ISO180006B        = 0x03
//TMR_TAG_PROTOCOL_GEN2              = 0x05
//TMR_TAG_PROTOCOL_ISO180006B_UCODE  = 0x06
//TMR_TAG_PROTOCOL_IPX64             = 0x07
//TMR_TAG_PROTOCOL_IPX256            = 0x08
//TMR_TAG_PROTOCOL_ATA               = 0x1D
void RFID::setTagProtocol(uint8_t protocol)
{
  uint8_t data[2];
  data[0] = 0; //Opcode expects 16-bits
  data[1] = protocol;

  sendMessage(TMR_SR_OPCODE_SET_TAG_PROTOCOL, data, sizeof(data));
}

void RFID::enableReadFilter(void)
{
  setReaderConfiguration(0x0C, 0x01); //Enable read filter
}

//Disabling the read filter allows continuous reading of tags
void RFID::disableReadFilter(void)
{
  setReaderConfiguration(0x0C, 0x00); //Diable read filter
}

//Sends optional parameters to the module
//See TMR_SR_Configuration in serial_reader_imp.h for a breakdown of options
void RFID::setReaderConfiguration(uint8_t option1, uint8_t option2)
{
  uint8_t data[3];

  //These are parameters gleaned from inspecting the 'Transport Logs' of the Universal Reader Assistant
  //And from serial_reader_l3.c
  data[0] = 1; //Key value form of command
  data[1] = option1;
  data[2] = option2;

  sendMessage(TMR_SR_OPCODE_SET_READER_OPTIONAL_PARAMS, data, sizeof(data));
}

//Gets optional parameters from the module
//We know only the blob and are not able to yet identify what each parameter does
void RFID::getOptionalParameters(uint8_t option1, uint8_t option2)
{
  //These are parameters gleaned from inspecting the 'Transport Logs' of the Universal Reader Assistant
  //During setup the software pings different options
  uint8_t data[2];
  data[0] = option1;
  data[1] = option2;
  sendMessage(TMR_SR_OPCODE_GET_READER_OPTIONAL_PARAMS, data, sizeof(data));
}

//Get the version number from the module
void RFID::getVersion(void)
{
  sendMessage(TMR_SR_OPCODE_VERSION);
}

//Set the read TX power
//Maximum power is 2700 = 27.00 dBm
//1005 = 10.05dBm
void RFID::setReadPower(int16_t powerSetting)
{
  if (powerSetting > 2700)
    powerSetting = 2700; //Limit to 27dBm

  //Copy this setting into a temp data array
  uint8_t size = sizeof(powerSetting);
  uint8_t data[size];
  for (uint8_t x = 0; x < size; x++)
    data[x] = (uint8_t)(powerSetting >> (8 * (size - 1 - x)));

  sendMessage(TMR_SR_OPCODE_SET_READ_TX_POWER, data, size);
}

//Get the read TX power
void RFID::getReadPower()
{
  uint8_t data[] = {0x00}; //Just return power
  //uint8_t data[] = {0x01}; //Return power with limits

  sendMessage(TMR_SR_OPCODE_GET_READ_TX_POWER, data, sizeof(data));
}

//Set the write power
//Maximum power is 2700 = 27.00 dBm
//1005 = 10.05dBm
void RFID::setWritePower(int16_t powerSetting)
{
  uint8_t size = sizeof(powerSetting);
  uint8_t data[size];
  for (uint8_t x = 0; x < size; x++)
    data[x] = (uint8_t)(powerSetting >> (8 * (size - 1 - x)));

  sendMessage(TMR_SR_OPCODE_SET_WRITE_TX_POWER, data, size);
}

//Get the write TX power
void RFID::getWritePower()
{
  uint8_t data[] = {0x00}; //Just return power
  //uint8_t data[] = {0x01}; //Return power with limits

  sendMessage(TMR_SR_OPCODE_GET_WRITE_TX_POWER, data, sizeof(data));
}

//Read a single EPC
//Caller must provide an array for EPC to be stored in
uint8_t RFID::readTagEPC(uint8_t *epc, uint8_t &epcLength, uint16_t timeOut)
{
  uint8_t bank = 0x01;    //User data bank
  uint8_t address = 0x00; //Starts at 20

  return (readData(bank, address, epc, epcLength, timeOut));
}


//This reads the user data area of the tag. 0 to 64 bytes are normally available.
//Use with caution. The module can't control which tag hears the command.
//TODO Add support for accessPassword
uint8_t RFID::readUserData(uint8_t *userData, uint8_t &userDataLength, uint16_t timeOut)
{
  uint8_t bank = 0x03;    //User data bank
  uint8_t address = 0x00; //Starts at 0

  return (readData(bank, address, userData, userDataLength, timeOut));
}

//This writes data to the tag. 0, 4, 16 or 64 bytes may be available.
//Writes to the first spot 0x00 and fills up as much of the bytes as user provides
//Use with caution. Function doesn't control which tag hears the command.
uint8_t RFID::writeUserData(uint8_t *userData, uint8_t userDataLength, uint16_t timeOut)
{
  uint8_t bank = 0x03; //User memory
  uint8_t address = 0x00;

  return (writeData(bank, address, userData, userDataLength, timeOut));
}

//Write the kill password. Should be 4 bytes long
uint8_t RFID::writeKillPW(uint8_t *password, uint8_t passwordLength, uint16_t timeOut)
{
  uint8_t bank = 0x00;    //Passwords bank
  uint8_t address = 0x00; //Kill password address

  return (writeData(bank, address, password, passwordLength, timeOut));
}

//Read the kill password. Should be 4 bytes long
uint8_t RFID::readKillPW(uint8_t *password, uint8_t &passwordLength, uint16_t timeOut)
{
  uint8_t bank = 0x00;    //Passwords bank
  uint8_t address = 0x00; //Kill password address

  return (readData(bank, address, password, passwordLength, timeOut));
}

//Write the access password. Should be 4 bytes long
uint8_t RFID::writeAccessPW(uint8_t *password, uint8_t passwordLength, uint16_t timeOut)
{
  uint8_t bank = 0x00;    //Passwords bank
  uint8_t address = 0x02; //Access password address

  return (writeData(bank, address, password, passwordLength, timeOut));
}

//Read the access password. Should be 4 bytes long
uint8_t RFID::readAccessPW(uint8_t *password, uint8_t &passwordLength, uint16_t timeOut)
{
  uint8_t bank = 0x00;    //Passwords bank
  uint8_t address = 0x02; //Access password address

  return (readData(bank, address, password, passwordLength, timeOut));
}

//Read the unique TID of the tag. Should be 20 bytes long
//This is a depricated function left in place in case users still use the readTID command
//This function is actually reading the UID. To read the TID, including the Chip Vendor
//change the address to 0x00.
uint8_t RFID::readTID(uint8_t *tid, uint8_t &tidLength, uint16_t timeOut)
{
  uint8_t bank = 0x02; //Bank for TID
  uint8_t address = 0x02;

  return (readData(bank, address, tid, tidLength, timeOut));
}

//Read the unique ID of the tag. Can vary from 0 to 20 or more bytes
uint8_t RFID::readUID(uint8_t *tid, uint8_t &tidLength, uint16_t timeOut)
{
  uint8_t bank = 0x02;    //Bank for TID
  uint8_t address = 0x02; //UID of the TID starts at 4

  return (readData(bank, address, tid, tidLength, timeOut));
}


/** struct initializers **/
/*
// Initialize a ReadConfig struct
TagFilter RFID::initReadConfig(uint16_t searchFlag, uint16_t metadataFlag, bool continuous, uint16_t offtime, uint16_t streamStats, bool readNTags, uint16_t N) 
{
  return ReadConfig{searchFlag, metadataFlag, continuous, offtime, streamStats, readNTags, N};
}

// Initialize a TagFilter struct
TagFilter RFID::initTagFilter(uint8_t type, uint32_t password, uint32_t start, uint8_t filterDataBitLength, 
  uint8_t *filterData, uint8_t target, uint8_t action=, bool inverse, bool multiselect, bool secure) 
{
  return TagFilter{type, password, start, filterDataBitLength, filterData, target, action, inverse, multiselect, secure};
}*/

TagFilter RFID::initEmptyFilter()
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_SELECT_DISABLED, 0x00, 0x00, 0x00, NULL, 0x04, 0x00, false, false, false, false};
}


//EPC Read filter uses an address and sets the metadata flag
//EPCLength is in bytes
//Should be good for any read operation
TagFilter RFID::initEPCReadFilter(uint8_t *EPC, uint16_t EPCLength)
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_ADDRESSED_EPC, 0x00, 0x20, EPCLength * 8, EPC, 0x04, 0x00, false, true, false, true};
  /*
  TagFilter filter;
  filter.type = TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_ADDRESSED_EPC;
  filter.start = 0x20;
  filter.filterData = EPC;
  filter.filterDataBitLength = EPCLength * 8; //length is in bits
  filter.isMultiselect = true;
  filter.useMetadata = true;
  return filter;
  */
}

TagFilter RFID::initEPCWriteFilter(uint8_t *EPC, uint16_t EPCLength)
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_EPC, 0x00, 0x00, EPCLength * 8, EPC, 0x04, 0x00, false, false, false, false};  
}

TagFilter RFID::initEPCSingleReadFilter(uint8_t *EPC, uint16_t EPCLength)
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_EPC, 0x00, 0x00, EPCLength * 8, EPC, 0x04, 0x00, false, false, false, true};
}

//Filters out all tags with EPCBitLength
//Start address and invert has no effect
//Should work for any read operation
TagFilter RFID::initEPCLengthReadFilter(uint16_t EPCBitLength)
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_LENGTH_OF_EPC, 0x00, 0x00, EPCBitLength, NULL, 0x04, 0x00, false, true, false, true};
  /*
  TagFilter filter;
  filter.type = TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_LENGTH_OF_EPC;
  filter.filterDataBitLength = EPCBitLength;
  filter.isMultiselect = true;
  filter.useMetadata = true;
  return filter;
  */
}


//This read filter filters based on the data bank, starting at address 0x00
TagFilter RFID::initUserDataReadFilter(uint8_t *data, uint16_t dataLength)
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_USER_MEM, 0x00, 0x00, dataLength * 8, data, 0x04, 0x00, false, true, false, true};
  //TagFilter filter;
  //filter.type = TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_USER_MEM;
  //filter.filterData = data;
  //filter.filterDataBitLength = dataLength * 8; // length in bits
  //filter.isMultiselect = true;
  //filter.useMetadata = true;
  //return filter;
}

//Read filter to filter on the TID bank
TagFilter RFID::initTIDReadFilter(uint8_t *tid, uint16_t tidLength)
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_TID, 0x00, 0x00, tidLength * 8, tid, 0x04, 0x00, false, true, false, true};
  //TagFilter filter;
  //filter.type = TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_TID;
  //filter.filterData = tid;
  //filter.filterDataBitLength = tidLength * 8; // size in bits
  //filter.isMultiselect = true;
  //filter.useMetadata = true;
  //return filter;
}

//A password filter is applied when a non 0x00 password is supplied but no filter is specified
//Note: have not found any real use till date
TagFilter RFID::initPasswordFilter(uint32_t password)
{
  return TagFilter{TMR_SR_GEN2_SINGULATION_OPTION_USE_PASSWORD, password, 0x00, 0x00, NULL, 0x04, 0x00, false, true, false, false};
  //TagFilter filter;
  //filter.type = TMR_SR_GEN2_SINGULATION_OPTION_USE_PASSWORD;
  //filter.password = password;
  //filter.isMultiselect = true;
  //return filter;
}


/** message constructors **/

//Creates a filter that can be used for reading and writing (Gen2 only)
//Supports all types of filters(see the Universal Reader Assistant for more details) with exception of MultiFilters
//Check out the filterbytes function in serial_reader_l3.c:4489 (mercury API) for more details
//Password is always enabled
//Unknown support for SecureAccess(don't know what it does)
//Returns the right option based on the type and wether or not it was inversed
uint8_t RFID::constructFilterMsg(uint8_t *data, uint8_t &i, TagFilter &filter) 
{
  // return 0x00 if empty
  if(filter.type == TMR_SR_GEN2_SINGULATION_OPTION_SELECT_DISABLED)
  {
    return TMR_SR_GEN2_SINGULATION_OPTION_SELECT_DISABLED;
  }
  uint8_t option = filter.type;

  // always add a password (default 0x00000000) unless a length filter is provided
  if(filter.type != TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_LENGTH_OF_EPC)
  {
    for (uint8_t x = 0; x < sizeof(filter.password); x++)
    {
      data[i++] = filter.password >> (8 * (3 - x)) & 0xFF;    
    }
  }

  // GEN2 filters
  if(filter.type == TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_EPC)
  {
    // set extended flag if longer than 255
    if(filter.filterDataBitLength > 255)
    {
      option |= TMR_SR_GEN2_SINGULATION_OPTION_EXTENDED_DATA_LENGTH;
      data[i++] = filter.filterDataBitLength >> 8 & 0xFF;
    }
    data[i++] = filter.filterDataBitLength & 0xFF;
    // add filter data
    for (uint8_t x = 0; x < (filter.filterDataBitLength >> 3); x++)
    {
      data[i++] = filter.filterData[x];
    }
  }
  else
  {
    if(filter.type == TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_ADDRESSED_EPC || filter.type == TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_TID || 
      filter.type == TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_USER_MEM || filter.type == TMR_SR_GEN2_SINGULATION_OPTION_SELECT_GEN2TRUNCATE)
    {
      // set start address
      for (uint8_t x = 0; x < sizeof(filter.start); x++)
      {
        data[i++] = filter.start >> (8 * (3 - x)) & 0xFF;
      }

      // set extended flag if longer than 255
      if(filter.filterDataBitLength > 255)
      {
        option |= TMR_SR_GEN2_SINGULATION_OPTION_EXTENDED_DATA_LENGTH;
        data[i++] = filter.filterDataBitLength >> 8 & 0xFF;
      }
      data[i++] = filter.filterDataBitLength & 0xFF;
      // add filter data
      for (uint8_t x = 0; x < (((filter.filterDataBitLength - 1) >> 3) + 1); x++)
      {
        data[i++] = filter.filterData[x];
      }
    }
    // GEN2 length filter
    else if(filter.type == TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_LENGTH_OF_EPC)
    {
      data[i++] = filter.filterDataBitLength >> 8 & 0xFF;
      data[i++] = filter.filterDataBitLength & 0xFF;
    }
    // seems to be on most of the time
    if(filter.isMultiselect)
    {
      data[i++] = filter.target;
      data[i++] = filter.action;
      data[i++] = 0x00;
    }
    // if it was a password filter, return now
    if(filter.type == TMR_SR_GEN2_SINGULATION_OPTION_USE_PASSWORD)
    {
      return TMR_SR_GEN2_SINGULATION_OPTION_USE_PASSWORD;
    }
  }
  // change the type, does not get applied if it is a length filter
  if(filter.isInverse && filter.type != TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_LENGTH_OF_EPC)
  {
    option |= TMR_SR_GEN2_SINGULATION_OPTION_INVERSE_SELECT_BIT;
  }
  // not sure what this option does!
  if(filter.isSecure)
  {
    option |= TMR_SR_GEN2_SINGULATION_OPTION_SECURE_READ_DATA;
  }
  // reads usually want metadata as well
  if(filter.useMetadata) 
  {
    option |= TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA;
  }
  return option;
}

//Sets a default read config for reading multiple tags once
//metadata 0x57, search flag=0x13
ReadConfig RFID::initStandardReadMultipleTagsOnceConfig()
{
  ReadConfig config;
  config.metadataFlag = 0x57;
  config.searchFlag = 0x13;
  config.multiSelect = 0x88;
  return config;
}

/*
//Sets a default read config for reading one tag once
//metadata 0x57, search flag=0x13
ReadConfig RFID::initStandardReadSingleTagOnceConfig()
{
  ReadConfig config;
  config.metadataFlag = 0x57;
  config.searchFlag = 0x13;
  config.multiSelect = 0x00;
  return config;
}*/


//Default settings for the continuous read
//Offtime is set to 250 (1000/1250 * 100 = 80% utilisation)
ReadConfig RFID::initStandardContinuousReadConfig()
{
  ReadConfig config;
  config.isContinuous = true;
  config.offtime = 250; // 1000 on 200 off, reduces overheating
  config.searchFlag = 0x051B; // DUTY_CYCLE_CONTROL + STATS_REPORT_STREAMING + LARGE_TAG_POPULATION_SUPPORT + TAG_STREAMING + 3
  config.multiSelect = 0x88; // select multiple tags
  config.metadataFlag = 0x57; // select PROTOCOL + TIMESTAMP + ANTENNAID + RSSI + READCOUNT
  config.streamStats = 0x0100; 
  return config;
}

//Actually does not need anything else, default values are good
ReadConfig RFID::initStandardReadTagDataOnce()
{
  ReadConfig config;
  return config;
}


//Creates a message for the 0x22 (READ_TAG_ID_MULTIPLE) opcode
void RFID::constructReadTagIdMultipleMsg(uint8_t *data, uint8_t &i, ReadConfig &readConfig, struct TagFilter &filter, uint16_t timeOut)
{
  //Format:
  //  1 Byte   1 Byte   1 Byte   1 Byte         1 Byte        2 Bytes       2 Bytes   2 Bytes        2 Bytes    2 Bytes        4 Bytes                2 Bytes
  //| Header | Length | Opcode | Tag Op. Mode | Option Byte | Search Flag | Timeout | D.C. Offtime | Metadata | Stream Stats | Nr. of Tags | Filter | Checksum |
  //D.C Offtime and Stream Stats are only set in continuous mode
  //Nr of tags is only used in N-tags mode
  
  //Example without filter
  //FF            = Header
  //08            = Length
  //22            = Opcode
  //88            = Tag operation mode (multiselect)
  //10            = Option bytes (TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA)
  //00 13         = Search flag
  //03 E8         = Timeout
  //00 57         = Metadata flag (PROTOCOL + TIMESTAMP + RSSI + ANTENNA ID + READCOUNT)
  //1F DF         = Checksum 

  //Example with filter
  //FF                = Header
  //1A                = Length
  //22                = Opcode
  //88                = Tag operation mode (multiselect)
  //14                = Option bytes (TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA)
  //00 13             = Search flag
  //03 E8             = Timeout
  //00 57             = Metadata flag (PROTOCOL + TIMESTAMP + RSSI + ANTENNA ID + READCOUNT)
  //00 00 00 00       = access password --> filter start
  //00 00 00 20       = bit pointer start address 
  //30                = filter length  
  //AA BB CC DD EE FF = filter data
  //04                = target
  //00                = action
  //00                = End of select indicator --> filter end
  //05 70             = Checksum
  uint8_t optionByte; 

  // do a size check here

  // set multiselect if enabled
  if(readConfig.multiSelect != 0x00)
    data[i++] = readConfig.multiSelect;
  optionByte = i;
  data[i++] = 0x00; // option byte
  data[i++] = readConfig.searchFlag >> 8 & 0xFF;
  data[i++] = readConfig.searchFlag & 0xFF;

  //Insert timeout
  data[i++] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[i++] = timeOut & 0xFF;      //Timeout lsB in ms

  // set off time if continuous scanning is enabled
  if(readConfig.isContinuous)
  {
    data[i++] = readConfig.offtime >> 8 & 0xFF;
    data[i++] = readConfig.offtime & 0XFF;
  }

  //Insert metadata to collect
  data[i++] = readConfig.metadataFlag >> 8 & 0xFF;
  data[i++] = readConfig.metadataFlag & 0xFF;

  // insert stream stats
  if(readConfig.isContinuous)
  {
    data[i++] = readConfig.streamStats >> 8 & 0xFF;
    data[i++] = readConfig.streamStats & 0xFF;
  }
  if(readConfig.readNTags)
  {
    data[i++] = readConfig.N >> 8 & 0xFF;
    data[i++] = readConfig.N & 0xFF;    
  }
  data[optionByte] = constructFilterMsg(data, i, filter) | TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA;
}


//Creates a message for the 0x22 (READ_TAG_ID_MULTIPLE) opcode
void RFID::constructReadTagDataMsg(uint8_t *data, uint8_t &i, uint8_t bank, uint32_t address, uint8_t dataLengthRead, ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut)
{
  //Format:
  //  1 Byte   1 Byte   1 Byte   2 Byte    1 Byte        1 Bytes       2 Bytes   1 Bytes    4 Bytes   1 Bytes            2 Bytes
  //| Header | Length | Opcode | Timeout | Multiselect | Option Byte | Metadata | Bank    | Address | Length  | Filter | Checksum |
  //D.C Offtime and Stream Stats are only set in continuous mode
  //Nr of tags is only used in N-tags mode
  
  //Example with filter without multi select
  //FF                                  = header
  //1C                                  = length 
  //28                                  = opcode  
  //03 E8                               = time out  
  //11                                  = option byte 
  //00 00                               = metadata (no metadata)
  //02                                  = bank
  //00 00 00 00                         = start address  
  //00                                  = nr of bytes to read (0 means all)
  //00 00 00  00                        = access password --> filter start  
  //60                                  = filter size in bits
  //98 12 AB 32 FF 00 02 06 05 E4 01 F6 = filter data --> filter end
  //01  48                              = checksum
  uint8_t optionByte; 

  // do a size check here

  data[i++] = timeOut >> 8 & 0xFF;
  data[i++] = timeOut & 0xFF;
  if(readConfig.multiSelect != 0x00)
  {
    data[i++] = readConfig.multiSelect; // multiselect
  }
  optionByte = i;
  data[i++] = 0x00; // option byte

  //Insert metadata
  if(filter.useMetadata)
  {
    data[i++] = readConfig.metadataFlag >> 8 & 0xFF; 
    data[i++] = readConfig.metadataFlag & 0xFF;     
  }
  data[i++] = bank;

  //Splice address into array
  for (uint8_t x = 0; x < sizeof(address); x++)
    data[i++] = address >> (8 * (3 - x)) & 0xFF;
  if (bank == 0x03)
    data[i++] = 0x00; // read the whole bank
  else
    data[i++] = dataLengthRead / 2; //Number of 16-bit chunks to read.
  data[optionByte] = constructFilterMsg(data, i, filter) | TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA;
}

//This function tests if the command was executed properly
uint8_t RFID::checkResponse(uint8_t *dataRead, uint8_t &dataLengthRead) 
{
  if (msg[0] == ALL_GOOD) //We received a good response
  {
    uint16_t status = (msg[3] << 8) | msg[4];

    if (status == 0x0000)
    {
      uint8_t responseLength = msg[1];

      if (responseLength < dataLengthRead) //User wants us to read more than we have available
        dataLengthRead = responseLength;

      //There is a case here where responseLegnth is more than dataLengthRead, in which case we ignore (don't load) the additional bytes
      //Load limited response data into caller's array
      for (uint8_t x = 0; x < dataLengthRead; x++)
        dataRead[x] = msg[5 + x];

      return (RESPONSE_SUCCESS);
    }
  }

  //Else - msg[0] was timeout or other
  dataLengthRead = 0; //Inform caller that we weren't able to read anything

  return (RESPONSE_FAIL);
}

// Creates the WriteEPC messages (0x23)
void RFID::constructWriteTagIdMsg(uint8_t *data, uint8_t &i, uint8_t *newEPC, uint8_t newEPCLength, TagFilter &filter, uint16_t timeOut)
{
  //Examples taken from URA
  //Example without filter:
  //FF                      = header  
  //10                      = length 
  //23                      = opcode 
  //03  E8                  = timeout 
  //00                      = filter disabled
  //00                      = filter size in bits
  //98  12  AB  32  FF  00  = new epc
  //D2  F7                  = checksum
  
  //Example with filter:
  //FF                      = header
  //12                      = length
  //23                      = opcode 
  //03  E8                  = timeout
  //01                      = filter option byte --> TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_EPC
  //00  00  00  00          = password 
  //20                      = filter size in bits
  //AA  BB  CC  DD          = old EPC 
  //AA  BB  CC  DD  EE  FF  = new EPC
  //2F  FA                  = checksum 

  uint8_t optionByte;
  //Pre-load array options
  data[i++] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[i++] = timeOut & 0xFF;      //Timeout lsB in ms
  // temporarily set the option byte
  optionByte = i;
  data[i++] = 0x00;
  // add filter if enabled
  data[optionByte] = constructFilterMsg(data, i, filter); 
  //Splice data into array
  for (uint8_t x = 0; x < newEPCLength; x++)
    data[i++] = newEPC[x];
}
void RFID::constructWriteTagDataMsg(uint8_t *data, uint8_t &i, uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, TagFilter &filter, uint16_t timeOut)
{
  //Filter disabled
  //Example: FF  0A  24  03  E8  00  00  00  00  00  03  00  EE  58  9D
  //FF 0A 24 = Header, LEN, Opcode
  //03 E8 = Timeout in ms
  //00 = Option initialize
  //00 00 00 00 = Address
  //03 = Bank
  //00 EE = Data
  //58 9D = CRC

  //Filter enabled example
  //FF                  = header
  //15                  = length
  //24                  = opcode
  //03 E8               = timeout
  //01                  = filter option byte
  //00 00 00 00         = address
  //03                  = bank
  //00 00 00 00         = password
  //30                  = length of EPC filter in bits
  //AA BB CC DD EE FF   = EPC filter
  //CC FF               = data 
  //06 15               = checksum
  //Note: URA writes 2 bytes every time

  uint8_t optionByte;
  //Pre-load array options
  data[i++] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[i++] = timeOut & 0xFF;      //Timeout lsB in ms
  optionByte = i;
  data[i++] = 0x00;                //Option initialize

  //Splice address into array
  for (uint8_t x = 0; x < sizeof(address); x++)
    data[i++] = address >> (8 * (3 - x)) & 0xFF;

  //Bank 0 = Passwords
  //Bank 1 = EPC Memory Bank
  //Bank 2 = TID
  //Bank 3 = User Memory
  data[i++] = bank;
  // add filter (does nothing if no filter is provided)
  data[optionByte] = constructFilterMsg(data, i, filter); 
  //Splice data into array
  for (uint8_t x = 0; x < dataLengthToRecord; x++)
    data[i++] = dataToRecord[x];
}

uint8_t RFID::writeTagEPC(uint8_t *newID, uint8_t newIDLength, uint16_t timeOut)
{
  TagFilter filter = initEmptyFilter();
  return writeTagEPCWithFilter(newID, newIDLength, filter, timeOut);
}

//Writes an EPC to the first tag that satisfies the filter
//User needs to make sure that the EPCs are unique
uint8_t RFID::writeTagEPCWithFilter(uint8_t *newEPC, uint8_t newEPCLength, TagFilter &filter, uint16_t timeOut)
{
  uint8_t i = 0; 
  uint8_t data[MAX_MSG_SIZE];
  constructWriteTagIdMsg(data, i, newEPC, newEPCLength, filter, timeOut);
  sendMessage(TMR_SR_OPCODE_WRITE_TAG_ID, data, i, timeOut);

  if (msg[0] == ALL_GOOD) //We received a good response
  {
    uint16_t status = (msg[3] << 8) | msg[4];

    if (status == 0x0000)
      return (RESPONSE_SUCCESS);
  }

  //Else - msg[0] was timeout or other
  return (RESPONSE_FAIL);
}

uint8_t RFID::writeData(uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, uint16_t timeOut)
{
  TagFilter filter;
  return writeDataWithFilter(bank, address, dataToRecord, dataLengthToRecord, filter, timeOut);
}

//Writes a data array to a given bank and address
//Allows for writing of passwords and user data
uint8_t RFID::writeDataWithFilter(uint8_t bank, uint32_t address, uint8_t *dataToRecord, uint8_t dataLengthToRecord, TagFilter &filter, uint16_t timeOut)
{
  uint8_t i = 0; 
  uint8_t data[MAX_MSG_SIZE];
  uint8_t option = TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_EPC;
  constructWriteTagDataMsg(data, i, bank, address, dataToRecord, dataLengthToRecord, filter, timeOut);
  sendMessage(TMR_SR_OPCODE_WRITE_TAG_DATA, data, i, timeOut);

  if (msg[0] == ALL_GOOD) //We received a good response
  {
    uint16_t status = (msg[3] << 8) | msg[4];

    if (status == 0x0000)
      return (RESPONSE_SUCCESS);
  }

  //Else - msg[0] was timeout or other
  return (RESPONSE_FAIL);
}

//Reads all the tag IDs it sees
uint8_t RFID::readMultipleTags(uint16_t timeOut)
{
  ReadConfig config = initStandardReadMultipleTagsOnceConfig();
  TagFilter filter = initEmptyFilter();
  return readMultipleTagsWithFilterConfig(config, filter, timeOut);
}

/*
//Reads all the tag IDs it sees
uint8_t RFID::readSingleTag(uint16_t timeOut)
{
  ReadConfig config = initStandardReadSingleTagOnceConfig();
  TagFilter filter = initEmptyFilter();
  return readMultipleTagsWithFilterConfig(config, filter, timeOut);
}*/

//Reads all the tag IDs it sees
uint8_t RFID::readMultipleTagsWithFilterConfig(ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut)
{
  uint8_t i = 0; 
  uint8_t data[MAX_MSG_SIZE];

  // clear buffer(FF 00 2A 1D 25) empty message
  sendMessage(TMR_SR_OPCODE_CLEAR_TAG_ID_BUFFER, NULL, 0);
  // construct main read to buffer message
  constructReadTagIdMultipleMsg(data, i, readConfig, filter, timeOut);
  sendMessage(TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE, data, i, timeOut);
  // test if tags were found
  if(msg[3] == 0x00 && msg[4] == 0x00) 
  {
    // retrieve from buffer
    // TODO handle responses larger than 255
    byte buffercmd[3];
    buffercmd[0] = readConfig.metadataFlag >> 8 & 0xFF;
    buffercmd[1] = readConfig.metadataFlag & 0xFF;
    buffercmd[2] = 0x00; 
    sendMessage(TMR_SR_OPCODE_GET_TAG_ID_BUFFER, buffercmd, sizeof(buffercmd));
  }
  // we don't need to fill buffers because we need to follow up with sendTagFromBuffer
  // just return if it was succesful or not
  return 0;
}

uint8_t RFID::readData(uint8_t bank, uint32_t address, uint8_t *dataRead, uint8_t &dataLengthRead, uint16_t timeOut)
{
  ReadConfig readConfig;
  TagFilter filter;
  return readDataWithFilterConfig(bank, address, dataRead, dataLengthRead, readConfig, filter, timeOut);
}

//Reads a given bank and address to a data array
//Allows for reading of passwords, EPCs, and user data
uint8_t RFID::readDataWithFilterConfig(uint8_t bank, uint32_t address, uint8_t *dataRead, uint8_t &dataLengthRead, ReadConfig &readConfig, TagFilter &filter, uint16_t timeOut)
{
  //Bank 0
  //response: [00] [08] [28] [00] [00] [EE] [FF] [11] [22] [12] [34] [56] [78]
  //[EE] [FF] [11] [22] = Kill pw
  //[12] [34] [56] [78] = Access pw

  //Bank 1
  //response: [00] [08] [28] [00] [00] [28] [F0] [14] [00] [AA] [BB] [CC] [DD]
  //[28] [F0] = CRC
  //[14] [00] = PC
  //[AA] [BB] [CC] [DD] = EPC

  //Bank 2
  //response: [00] [18] [28] [00] [00] [E2] [00] [34] [12] [01] [6E] [FE] [00] [03] [7D] [9A] [A3] [28] [05] [01] [6B] [00] [05] [5F] [FB] [FF] [FF] [DC] [00]
  //[E2] = CIsID
  //[00] [34] [12] = Vendor ID = 003, Model ID == 412
  //[01] [6E] [FE] [00] [03] [7D] [9A] [A3] [28] [05] [01] [69] [10] [05] [5F] [FB] [FF] [FF] [DC] [00] = Unique ID (TID)

  //Bank 3
  //response: [00] [40] [28] [00] [00] [41] [43] [42] [44] [45] [46] [00] [00] [00] [00] [00] [00] ...
  //User data

  uint8_t i = 0; 
  uint8_t data[MAX_MSG_SIZE];

  constructReadTagDataMsg(data, i, bank, address, dataLengthRead, readConfig, filter, timeOut);
  /*
  Serial.println(i);
  for(uint8_t x = 0; x < i; x++)
  {
    Serial.print(data[x], HEX);
    Serial.print(" ");
  }
  Serial.println("");
  return 0;
  */
  sendMessage(TMR_SR_OPCODE_READ_TAG_DATA, data, i, timeOut);
  return checkResponse(dataRead, dataLengthRead);
}

//Send the appropriate command to permanently kill a tag. If the password does not
//match the tag's pw it won't work. Default pw is 0x00000000
//Use with caution. This function doesn't control which tag hears the command.
//TODO Can we add ability to write to specific EPC?
uint8_t RFID::killTag(uint8_t *password, uint8_t passwordLength, uint16_t timeOut)
{
  uint8_t data[4 + passwordLength];

  data[0] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[1] = timeOut & 0xFF;      //Timeout lsB in ms
  data[2] = 0x00;                //Option initialize

  //Splice password into array
  for (uint8_t x = 0; x < passwordLength; x++)
    data[3 + x] = password[x];

  data[3 + passwordLength] = 0x00; //RFU

  sendMessage(TMR_SR_OPCODE_KILL_TAG, data, sizeof(data), timeOut);

  if (msg[0] == ALL_GOOD) //We received a good response
  {
    uint16_t status = (msg[3] << 8) | msg[4];

    if (status == 0x0000)
      return (RESPONSE_SUCCESS);
  }

  //Else - msg[0] was timeout or other
  return (RESPONSE_FAIL);
}

//Checks incoming buffer for the start characters
//Returns true if a new message is complete and ready to be cracked
bool RFID::check()
{
  while (_nanoSerial->available())
  {
    uint8_t incomingData = _nanoSerial->read();

    //Wait for header byte
    if (_head == 0 && incomingData != 0xFF)
    {
      //Do nothing. Ignore this byte because we need a start byte
    }
    else
    {
      //Load this value into the array
      msg[_head++] = incomingData;

      _head %= MAX_MSG_SIZE; //Wrap variable

      if ((_head > 0) && (_head == msg[1] + 7))
      {
        //We've got a complete sentence!

        //Erase the remainder of the array
        for (uint8_t x = _head; x < MAX_MSG_SIZE; x++)
          msg[x] = 0;

        _head = 0; //Reset

        //Used for debugging: Does the user want us to print the command to serial port?
        if (_printDebug == true)
        {
          _debugSerial->print(F("response: "));
          printMessageArray();
        }

        return (true);
      }
    }
  }

  return (false);
}

//See parseResponse for breakdown of fields
//Pulls the number of EPC bytes out of the response
//Often this is 12 bytes
uint8_t RFID::getTagEPCBytes(void)
{
  uint16_t epcBits = 0; //Number of bits of EPC (including PC, EPC, and EPC CRC)

  uint8_t tagDataBytes = getTagDataBytes(); //We need this offset

  for (uint8_t x = 0; x < 2; x++)
    epcBits |= (uint16_t)msg[27 + tagDataBytes + x] << (8 * (1 - x));
  uint8_t epcBytes = epcBits / 8;
  epcBytes -= 4; //Ignore the first two bytes and last two bytes

  return (epcBytes);
}

//See parseResponse for breakdown of fields
//Pulls the number of data bytes out of the response
//Often this is zero
uint8_t RFID::getTagDataBytes(void)
{
  //Number of bits of embedded tag data
  uint8_t tagDataLength = 0;
  for (uint8_t x = 0; x < 2; x++)
    tagDataLength |= (uint16_t)msg[24 + x] << (8 * (1 - x));
  uint8_t tagDataBytes = tagDataLength / 8;
  if (tagDataLength % 8 > 0)
    tagDataBytes++; //Ceiling trick

  return (tagDataBytes);
}

//See parseResponse for breakdown of fields
//Pulls the timestamp since last Keep-Alive message from a full response record stored in msg
uint16_t RFID::getTagTimestamp(void)
{
  //Timestamp since last Keep-Alive message
  uint32_t timeStamp = 0;
  for (uint8_t x = 0; x < 4; x++)
    timeStamp |= (uint32_t)msg[17 + x] << (8 * (3 - x));

  return (timeStamp);
}

//See parseResponse for breakdown of fields
//Pulls the frequency value from a full response record stored in msg
uint32_t RFID::getTagFreq(void)
{
  //Frequency of the tag detected is loaded over three bytes
  uint32_t freq = 0;
  for (uint8_t x = 0; x < 3; x++)
    freq |= (uint32_t)msg[14 + x] << (8 * (2 - x));

  return (freq);
}

//See parseResponse for breakdown of fields
//Pulls the RSSI value from a full response record stored in msg
int8_t RFID::getTagRSSI(void)
{
  return (msg[12] - 256);
}

//This will parse whatever response is currently in msg into its constituents
//Mostly used for parsing out the tag IDs and RSSI from a multi tag continuous read
uint8_t RFID::parseResponse(void)
{
  //See http://www.thingmagic.com/images/Downloads/Docs/AutoConfigTool_1.2-UserGuide_v02RevA.pdf
  //for a breakdown of the response packet

  //Example response:
  //FF  28  22  00  00  10  00  1B  01  FF  01  01  C4  11  0E  16
  //40  00  00  01  27  00  00  05  00  00  0F  00  80  30  00  00
  //00  00  00  00  00  00  00  00  00  15  45  E9  4A  56  1D
  //  [0] FF = Header
  //  [1] 28 = Message length
  //  [2] 22 = OpCode
  //  [3, 4] 00 00 = Status
  //  [5 to 11] 10 00 1B 01 FF 01 01 = RFU 7 bytes
  //  [12] C4 = RSSI
  //  [13] 11 = Antenna ID (4MSB = TX, 4LSB = RX)
  //  [14, 15, 16] 0E 16 40 = Frequency in kHz
  //  [17, 18, 19, 20] 00 00 01 27 = Timestamp in ms since last keep alive msg
  //  [21, 22] 00 00 = phase of signal tag was read at (0 to 180)
  //  [23] 05 = Protocol ID
  //  [24, 25] 00 00 = Number of bits of embedded tag data [M bytes]
  //  [26 to M] (none) = Any embedded data
  //  [26 + M] 0F = RFU reserved future use
  //  [27, 28 + M] 00 80 = EPC Length [N bytes]  (bits in EPC including PC and CRC bits). 128 bits = 16 bytes
  //  [29, 30 + M] 30 00 = Tag EPC Protocol Control (PC) bits
  //  [31 to 42 + M + N] 00 00 00 00 00 00 00 00 00 00 15 45 = EPC ID
  //  [43, 44 + M + N] 45 E9 = EPC CRC
  //  [45, 46 + M + N] 56 1D = Message CRC

  uint8_t msgLength = msg[1] + 7; //Add 7 (the header, length, opcode, status, and CRC) to the LEN field to get total bytes
  uint8_t opCode = msg[2];

  //Check the CRC on this response
  uint16_t messageCRC = calculateCRC(&msg[1], msgLength - 3); //Ignore header (start spot 1), remove 3 bytes (header + 2 CRC)
  if ((msg[msgLength - 2] != (messageCRC >> 8)) || (msg[msgLength - 1] != (messageCRC & 0xFF)))
  {
    return (ERROR_CORRUPT_RESPONSE);
  }

  if (opCode == TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE) //opCode = 0x22
  {
    //Based on the record length identify if this is a tag record, a temperature sensor record, or a keep-alive?
    if (msg[1] == 0x00) //Keep alive
    {
      //We have a Read cycle reset/keep-alive message
      //Sent once per second
      uint16_t statusMsg = 0;
      for (uint8_t x = 0; x < 2; x++)
        statusMsg |= (uint32_t)msg[3 + x] << (8 * (1 - x));

      if (statusMsg == 0x0400)
      {
        return (RESPONSE_IS_KEEPALIVE);
      }
      else if (statusMsg == 0x0504)
      {
        return (RESPONSE_IS_TEMPTHROTTLE);
      }
      else
      {
        return (RESPONSE_IS_UNKNOWN);
      }
    }
    else if (msg[1] == 0x08) //Unknown
    {
      return (RESPONSE_IS_UNKNOWN);
    }
    else if (msg[1] == 0x0a) //temperature
    {
      return (RESPONSE_IS_TEMPERATURE);
    }
    else //Full tag record
    {
      //This is a full tag response
      //User can now pull out RSSI, frequency of tag, timestamp, EPC, Protocol control bits, EPC CRC, CRC
      return (RESPONSE_IS_TAGFOUND);
    }
  }
  else
  {
    if (_printDebug == true)
    {
      _debugSerial->print(F("Unknown opcode in response: 0x"));
      _debugSerial->println(opCode, HEX);
    }
    return (ERROR_UNKNOWN_OPCODE);
  }
}

//Given an opcode, a piece of data, and the size of that data, package up a sentence and send it
void RFID::sendMessage(uint8_t opcode, uint8_t *data, uint8_t size, uint16_t timeOut, boolean waitForResponse)
{
  msg[1] = size; //Load the length of this operation into msg array
  msg[2] = opcode;

  //Copy the data into msg array
  for (uint8_t x = 0; x < size; x++)
    msg[3 + x] = data[x];

  sendCommand(timeOut, waitForResponse); //Send and wait for response
}

//Given an array, calc CRC, assign header, send it out
//Modifies the caller's msg array
void RFID::sendCommand(uint16_t timeOut, boolean waitForResponse)
{
  msg[0] = 0xFF; //Universal header
  uint8_t messageLength = msg[1];

  uint8_t opcode = msg[2]; //Used to see if response from module has the same opcode

  //Attach CRC
  uint16_t crc = calculateCRC(&msg[1], messageLength + 2); //Calc CRC starting from spot 1, not 0. Add 2 for LEN and OPCODE bytes.
  msg[messageLength + 3] = crc >> 8;
  msg[messageLength + 4] = crc & 0xFF;

  //Used for debugging: Does the user want us to print the command to serial port?
  if (_printDebug == true)
  {
    _debugSerial->print(F("sendCommand: "));
    printMessageArray();
  }

  //Remove anything in the incoming buffer
  //TODO this is a bad idea if we are constantly readings tags
  while (_nanoSerial->available())
    _nanoSerial->read();

  //Send the command to the module
  for (uint8_t x = 0; x < messageLength + 5; x++)
    _nanoSerial->write(msg[x]);

  //There are some commands (setBaud) that we can't or don't want the response
  if (waitForResponse == false)
  {
    _nanoSerial->flush(); //Wait for serial sending to complete
    return;
  }

  //For debugging, probably remove
  //for (uint8_t x = 0 ; x < 100 ; x++) msg[x] = 0;

  //Wait for response with timeout
  uint32_t startTime = millis();
  while (_nanoSerial->available() == false)
  {
    if (millis() - startTime > timeOut)
    {
      if (_printDebug == true)
        _debugSerial->println(F("Time out 1: No response from module"));
      msg[0] = ERROR_COMMAND_RESPONSE_TIMEOUT;
      return;
    }
    delay(1);
  }

  // Layout of response in data array:
  // [0] [1] [2] [3]      [4]      [5] [6]  ... [LEN+4] [LEN+5] [LEN+6]
  // FF  LEN OP  STATUSHI STATUSLO xx  xx   ... xx      CRCHI   CRCLO
  messageLength = MAX_MSG_SIZE - 1; //Make the max length for now, adjust it when the actual len comes in
  uint8_t spot = 0;
  while (spot < messageLength)
  {
    if (millis() - startTime > timeOut)
    {
      if (_printDebug == true)
        _debugSerial->println(F("Time out 2: Incomplete response"));

      msg[0] = ERROR_COMMAND_RESPONSE_TIMEOUT;
      return;
    }

    if (_nanoSerial->available())
    {
      msg[spot] = _nanoSerial->read();

      if (spot == 1)                //Grab the length of this response (spot 1)
        messageLength = msg[1] + 7; //Actual length of response is ? + 7 for extra stuff (header, Length, opcode, 2 status bytes, ..., 2 bytes CRC = 7)

      spot++;

      //There's a case were we miss the end of one message and spill into another message.
      //We don't want spot pointing at an illegal spot in the array
      spot %= MAX_MSG_SIZE; //Wrap condition
    }
  }

  //Used for debugging: Does the user want us to print the command to serial port?
  if (_printDebug == true)
  {
    _debugSerial->print(F("response: "));
    printMessageArray();
  }

  //Check CRC
  crc = calculateCRC(&msg[1], messageLength - 3); //Remove header, remove 2 crc bytes
  if ((msg[messageLength - 2] != (crc >> 8)) || (msg[messageLength - 1] != (crc & 0xFF)))
  {
    msg[0] = ERROR_CORRUPT_RESPONSE;
    if (_printDebug == true)
      _debugSerial->println(F("Corrupt response"));
    return;
  }

  //If crc is ok, check that opcode matches (did we get a response to the command we sent or a different one?)
  if (msg[2] != opcode)
  {
    msg[0] = ERROR_WRONG_OPCODE_RESPONSE;
    if (_printDebug == true)
      _debugSerial->println(F("Wrong opcode response"));
    return;
  }

  //If everything is ok, load all ok into msg array
  msg[0] = ALL_GOOD;
}

//Print the current message array - good for debugging, looking at how the module responded
//TODO Don't hardcode the serial stream
void RFID::printMessageArray(void)
{
  if (_printDebug == true) //If user hasn't enabled debug we don't know what port to debug to
  {
    uint8_t amtToPrint = msg[1] + 5;
    if (amtToPrint > MAX_MSG_SIZE)
      amtToPrint = MAX_MSG_SIZE; //Limit this size

    for (uint16_t x = 0; x < amtToPrint; x++)
    {
      _debugSerial->print(" [");
      if (msg[x] < 0x10)
        _debugSerial->print("0");
      _debugSerial->print(msg[x], HEX);
      _debugSerial->print("]");
    }
    _debugSerial->println();
  }
}

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
uint16_t RFID::calculateCRC(uint8_t *u8Buf, uint8_t len)
{
  uint16_t crc = 0xFFFF;

  for (uint8_t i = 0; i < len; i++)
  {
    crc = ((crc << 4) | (u8Buf[i] >> 4)) ^ crctable[crc >> 12];
    crc = ((crc << 4) | (u8Buf[i] & 0x0F)) ^ crctable[crc >> 12];
  }

  return crc;
}