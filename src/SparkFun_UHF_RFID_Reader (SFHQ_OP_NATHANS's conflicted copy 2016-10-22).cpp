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
	tmr__status_8h.html - Contaings the Status Word error codes

  Functions to create:
    readTagID (TMR_SR_OPCODE_READ_TAG_ID_SINGLE)
    (done) startContiniousRead
    (done) readTagData
    (nyw) writeTagID
    (nyw) writeTagData
    (done) setReadTXPower / setWriteTXPower (or setPowerMode)
    (done) setRegion
    (done) serBaudRate
    lockTag
    (nyw) killTag
    (done) stopContiniousRead
    (kind of done) setOptionalParameters - enable read filter, iso configuration,
    readTemp (read temp of module)
*/

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "SparkFun_UHF_RFID_Reader.h"

RFID::RFID(void)
{
  // Constructor
}

//Initialize the Serial port
bool RFID::begin(uint8_t *msg, Stream &serialPort)
{
  _nanoSerial = &serialPort; //Grab which port the user wants us to use
  _nanoMsg = msg;

  //_nanoSerial->begin(); //Stream has no .begin() so the user has to do a whateverSerial.begin(xxxx); from setup()
}

//Set baud rate
//Takes in a baud rate
//Returns response in the msg array
void RFID::setBaud(long baudRate)
{
  //Copy this setting into a temp data array
  uint8_t size = sizeof(baudRate);
  uint8_t data[size];
  for (uint8_t x = 0 ; x < size ; x++)
    data[x] = (uint8_t)(baudRate >> (8 * (size - 1 - x)));

  sendMessage(TMR_SR_OPCODE_SET_BAUD_RATE, data, size, false);
}

//Begin scanning for tags
//There are many many options and features to the nano, this sets options
//for continuous read of GEN2 type tags
void RFID::startReading()
{
  disableReadFilter(); //Don't filter for a specific tag, read all tags

  //This blob was found by using the 'Transport Logs' option from the Universal Reader Assistant
  //And connecting the Nano eval kit from Thing Magic to the URA
  //A lot of it has been deciphered but it's easier and faster just to pass a blob than to
  //assemble every option and sub-opcode.
  uint8_t configBlob[] = {0x00, 0x00, 0x01, 0x22, 0x00, 0x00, 0x05, 0x07, 0x22, 0x10, 0x00, 0x1B, 0x03, 0xE8, 0x01, 0xFF};

  sendMessage(TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP, configBlob, sizeof(configBlob));
}

//Stop a continuous read
void RFID::stopReading()
{
  //00 00 = Timeout, currently ignored
  //02 = Option - stop continuous reading
  uint8_t configBlob[] = {0x00, 0x00, 0x02};

  sendMessage(TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP, configBlob, sizeof(configBlob), false); //Do not wait for response
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
  //Copy this setting into a temp data array
  uint8_t size = sizeof(region);
  uint8_t data[size];
  for (uint8_t x = 0 ; x < size ; x++)
    data[x] = (uint8_t)(region >> (8 * (size - 1 - x)));

  sendMessage(TMR_SR_OPCODE_SET_REGION, data, size);
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
void RFID::setTagProtocol(uint16_t protocol)
{
  uint8_t data[2]; 
  data[0] = protocol >> 8 & 0xFF; //Opcode expects 16-bits
  data[1] = protocol & 0xFF;

  sendMessage(TMR_SR_OPCODE_SET_TAG_PROTOCOL, data, sizeof(data));
}

//This writes a new EPC to the first tag it detects
//Use with caution. This function doesn't control which tag hears the command.
void RFID::writeID(uint8_t *newID, uint8_t newIDLength, uint16_t timeOut)
{
  //FF  06  23  03  E8  00  00  AA  BB  62  4E - Write AA BB to tag
  //FF  0A  23  03  E8  00  00  AA  BB  CC  DD  EE  FF  F2  7E - Write AA BB CC DD EE FF to tag

  //FF  0C  23  03  E8  01  00  00  00  00  10  CC  DD  AA  BB  55  D3 - Write AABB to tag CCDD

  //Can you write really long IDs? Yes. Max I've written is 20 bytes. 12 or less is recommended

  uint8_t data[4 + newIDLength];

  //Pre-load array with magicBlob
  data[0] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[1] = timeOut & 0xFF; //Timeout lsB in ms
  data[2] = 0x00; //RFU
  data[3] = 0x00;

  //Dovetail new EPC ID onto blob
  for (uint8_t x = 0 ; x < newIDLength ; x++)
    data[4 + x] = newID[x];

  sendMessage(TMR_SR_OPCODE_WRITE_TAG_ID, data, sizeof(data));
}

//This writes data to the tag. 64 bytes are normally available.
//Writes to the first spot 0x00 and fills up as much of the 64 bytes as user provides
//Use with caution. The module can't control which tag hears the command.
//TODO Add support for accessPassword
//TODO Add support for writing to specific tag
//TODO Maybe add support for writing to specific spot
void RFID::writeUserData(uint8_t *userData, uint8_t userDataLength, uint16_t timeOut)
{
  //Example: FF  0A  24  03  E8  00  00  00  00  00  03  00  EE  58  9D
  //FF 0A 24 = Header, LEN, Opcode
  //03 E8 = Timeout in ms
  //00 = Option initialize
  //00 00 00 00 = Address
  //03 = Bank
  //00 EE = Data
  //58 9D = CRC

  //Bank 0 = Passwords
  //Bank 1 = EPC Memory Bank
  //Bank 2 = TID
  //Bank 3 = User Memory

  uint8_t data[8 + userDataLength];

  //Pre-load array with magicBlob
  data[0] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[1] = timeOut & 0xFF; //Timeout lsB in ms
  data[2] = 0x00; //Option initialize
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x03; //Bank 3 for user data

  //Dovetail new data onto blob
  for (uint8_t x = 0 ; x < userDataLength ; x++)
    data[8 + x] = userData[x];

  sendMessage(TMR_SR_OPCODE_WRITE_TAG_DATA, data, sizeof(data));
}

//This reads the user data area of the tag. 64 bytes are normally available.
//Use with caution. The module can't control which tag hears the command.
//TODO Add support for accessPassword
//TODO Add support for reading a specific tag
void RFID::readUserData(uint8_t *epc, uint16_t epcLength, uint16_t timeOut)
{
  //Example: FF  12  28  03  E8  11  00  00  03  00  00  00  00  00  00  00  00  00  10  AA  BB  1E  F7
  //FF 12 28 = Header, LEN, Opcode
  //03 E8 = Timeout in ms
  //11 = Options
  //00 00 = Meta data flags
  //03 = Bank
  //00 00 00 00 = Word address
  //00 = Length
  //00 00 00 00 = ?
  //10 = ?
  //AA BB = Tag ID to read
  //1E F7 = CRC
  
  uint8_t data[16 + epcLength];

  //Clear array
  for(uint8_t x = 0 ; x < sizeof(data) ; x++)
    data[x] = 0;

  //Pre-load array with magicBlob
  data[0] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[1] = timeOut & 0xFF; //Timeout lsB in ms
  data[2] = 0x11; //Options
  data[5] = 0x03; //Bank 3
  data[15] = 0x10; //Unknown
  //data[16] = 0xAA; //Tag ID
  //data[17] = 0xBB; //Tag ID

  //Dovetail EPC onto blob
  for (uint8_t x = 0 ; x < epcLength ; x++)
    data[16 + x] = epc[x];

  sendMessage(TMR_SR_OPCODE_READ_TAG_DATA, data, sizeof(data));
}

//Send the appropriate command to permanently kill a tag. If the password does not 
//match the tag's pw it won't work. Default pw is 0x00000000
//Use with caution. This function doesn't control which tag hears the command.
void RFID::killTag(uint32_t pw, uint16_t timeOut)
{
  uint8_t data[8];

  data[0] = timeOut >> 8 & 0xFF; //Timeout msB in ms
  data[1] = timeOut & 0xFF; //Timeout lsB in ms
  data[2] = 0x00; //Option initialize

  //Splice password into array
  for (uint8_t x = 0 ; x < sizeof(pw) ; x++)
    data[3 + x] = pw >> (8*(3-x)) & 0xFF;

  data[7] = 0x00; //RFU

  sendMessage(TMR_SR_OPCODE_KILL_TAG, data, sizeof(data));
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

//Sends protocol parameters to the module
//We know only the blob and are not able to yet identify what each parameter does
void RFID::setProtocolParameters()
{
  //These are parameters gleaned from inspecting the 'Transport Logs' of the Universal Reader Assistant
  //FF  03  9B  05  02  02  DE  EA
  //FF  03  9B  05  12  00  CE  E8
  //FF  03  9B  05  10  00  CC  E8
  //FF  03  9B  05  11  00  CD  E8
  //FF  03  9B  05  00  00  DC  E8
  //FF  04  9B  05  01  01  00  A2  FD

  uint8_t configBlob1[] = {0x05, 0x02, 0x02};
  sendMessage(TMR_SR_OPCODE_SET_PROTOCOL_PARAM, configBlob1, sizeof(configBlob1));

  uint8_t configBlob2[] = {0x05, 0x12, 0x00};
  sendMessage(TMR_SR_OPCODE_SET_PROTOCOL_PARAM, configBlob2, sizeof(configBlob2));

  uint8_t configBlob3[] = {0x05, 0x10, 0x00};
  sendMessage(TMR_SR_OPCODE_SET_PROTOCOL_PARAM, configBlob3, sizeof(configBlob3));

  uint8_t configBlob4[] = {0x05, 0x11, 0x00};
  sendMessage(TMR_SR_OPCODE_SET_PROTOCOL_PARAM, configBlob4, sizeof(configBlob4));

  uint8_t configBlob5[] = {0x05, 0x00, 0x00};
  sendMessage(TMR_SR_OPCODE_SET_PROTOCOL_PARAM, configBlob5, sizeof(configBlob5));

  uint8_t configBlob6[] = {0x05, 0x01, 0x01, 00};
  sendMessage(TMR_SR_OPCODE_SET_PROTOCOL_PARAM, configBlob6, sizeof(configBlob6));
}

//Gets Protocol Parameters from the module
//We know only the blob and are not able to yet identify what each parameter does
void RFID::getProtocolParameters(uint8_t option1, uint8_t option2)
{
  //These are parameters gleaned from inspecting the 'Transport Logs' of the Universal Reader Assistant
  //During setup the software pings different options
  uint8_t data[2];
  data[0] = option1;
  data[1] = option2;
  sendMessage(TMR_SR_OPCODE_GET_PROTOCOL_PARAM, data, sizeof(data));
}

//Get the version number from the module
void RFID::getVersion(void)
{
  sendMessage(TMR_SR_OPCODE_VERSION);
}

//Set the read TX power
//Power is as follows: maximum power is 2700 = 27.00 dBm
//1050 = 10.5dBm
void RFID::setReadPower(int16_t powerSetting)
{
  if(powerSetting > 2700) powerSetting = 2700; //Limit to 27dBm
  
  //Copy this setting into a temp data array
  uint8_t size = sizeof(powerSetting);
  uint8_t data[size];
  for (uint8_t x = 0 ; x < size ; x++)
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
//Power is -32,768 to 32,767
void RFID::setWritePower(int16_t powerSetting)
{
  uint8_t size = sizeof(powerSetting);
  uint8_t data[size];
  for (uint8_t x = 0 ; x < size ; x++)
    data[x] = (uint8_t)(powerSetting >> (8 * (size - 1 - x)));

  sendMessage(TMR_SR_OPCODE_SET_WRITE_TX_POWER, data, size);
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
      _nanoMsg[_head++] = incomingData;

      _head %= MAX_MSG_SIZE; //Wrap variable

      if ((_head > 0) && (_head == _nanoMsg[1] + 7))
      {
        //We've got a complete sentence!

        //Erase the remainder of the array
        for (uint8_t x = _head ; x < MAX_MSG_SIZE ; x++)
          _nanoMsg[x] = 0;

        _head = 0; //Reset

        return (true);
      }
    }
  }

  return (false);
}

//Pulls the RSSI value from a full response record stored in msg
int RFID::getRSSI(void)
{
  return(_nanoMsg[12] - 256);
}


//This will parse whatever response is currently in _nanoMsg into its constituents
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

  uint8_t msgLength = _nanoMsg[1] + 7; //Add 7 (the header, length, opcode, status, and CRC) to the LEN field to get total bytes
  uint8_t opCode = _nanoMsg[2];

  //Check the CRC on this response
  uint16_t messageCRC = calculateCRC(&_nanoMsg[1], msgLength - 3 ); //Ignore header (start spot 1), remove 3 bytes (header + 2 CRC)
/*  if ((_nanoMsg[msgLength - 2] != (messageCRC >> 8)) || (_nanoMsg[msgLength - 1] != (messageCRC & 0xFF)))
  {
	//TODO remove all Serial print statements
    Serial.println("Bad Message CRC!");
    return (ERROR_CORRUPT_RESPONSE);
  }

  if (opCode == TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE) //opCode = 0x22
  {
    //Based on the record length identify if this is a tag record, a temperature sensor record, or a keep-alive?
    if (_nanoMsg[1] == 0x0A) //Temp record
    {
      //We have a temperature status message
      //float temperature = _nanoMsg[14];

      //Convert to F because I am a bad global citizen
      //temperature = (temperature * 1.8) + 32.0;

      //Serial.print("temperature: ");
      //Serial.print(temperature, 1);
      //Serial.println("F");
      return (RESPONSE_IS_TEMPERATURE);
    }
    else if (_nanoMsg[1] == 0x00) //Keep alive
    {
      //We have a Read cycle reset/keep-alive message
      //Sent once per second
      uint16_t statusMsg = 0;
      for (uint8_t x = 0 ; x < 2 ; x++)
        statusMsg |= (uint32_t)_nanoMsg[3 + x] << (8 * (1 - x));

      if (statusMsg == 0x0400)
      {
        return (RESPONSE_IS_KEEPALIVE);
      }
      else if (statusMsg == 0x0504)
      {
        return (RESPONSE_IS_TEMPTHROTTLE);
      }
    }
    else if (_nanoMsg[1] == 0x08) //Unknown
    {
      return (RESPONSE_IS_UNKNOWN);
    }
    else //Full tag record
    {
      return (RESPONSE_IS_TAGFOUND);

      //This is a full tag response, break it apart
      //Pull out RSSI, frequency of tag, timestamp, EPC, Protocol control bits, EPC CRC, CRC

      //Frequency of the tag detected is loaded over three bytes
      uint32_t freq = 0;
      for (uint8_t x = 0 ; x < 3 ; x++)
        freq |= (uint32_t)_nanoMsg[14 + x] << (8 * (2 - x));

      //Timestamp since last Keep-Alive message
      uint32_t timeStamp = 0;
      for (uint8_t x = 0 ; x < 4 ; x++)
        timeStamp |= (uint32_t)_nanoMsg[17 + x] << (8 * (3 - x));

      //Number of bits of embedded tag data
      uint16_t tagDataLength = 0;
      for (uint8_t x = 0 ; x < 2 ; x++)
        tagDataLength |= (uint16_t)_nanoMsg[24 + x] << (8 * (1 - x));
      uint8_t tagDataBytes = tagDataLength / 8;
      if (tagDataLength % 8 > 0) tagDataBytes++; //Ceiling trick

      //Read in data bytes
      uint8_t tagData[tagDataBytes];
      for (uint8_t x = 0 ; x < tagDataBytes ; x++)
        tagData[x] = _nanoMsg[26 + x];

      //Number of bits of EPC (including PC, EPC, and EPC CRC)
      uint16_t epcLength = 0;
      for (uint8_t x = 0 ; x < 2 ; x++)
        epcLength |= (uint16_t)_nanoMsg[27 + tagDataBytes + x] << (8 * (1 - x));
      uint8_t epcBytes = epcLength / 8;
      epcBytes -= 4; //Ignore the first two bytes and last two bytes

      //Load the EPC into its own array
      uint8_t epc[epcBytes];
      for (uint8_t x = 0 ; x < epcBytes ; x++)
        epc[x] = _nanoMsg[31 + tagDataBytes + x];

      Serial.print(" freq[");
      Serial.print(freq);
      Serial.print("]");

      Serial.print(" timeStamp[");
      Serial.print(timeStamp);
      Serial.print("]");

      if (tagDataBytes)
      {
        Serial.print(" tagDataBytes[");
        Serial.print(tagDataBytes);
        Serial.print("]");

        Serial.print(" tagData[");
        for (uint8_t x = 0 ; x < tagDataBytes ; x++)
        {
          if (tagData[x] < 0x10) Serial.print("0");
          Serial.print(tagData[x], HEX);
          Serial.print(" ");
        }
        Serial.print("]");
      }

      Serial.print(" epc[");
      for (uint8_t x = 0 ; x < epcBytes ; x++)
      {
        if (epc[x] < 0x10) Serial.print("0");
        Serial.print(epc[x], HEX);
        Serial.print(" ");
      }
      Serial.print("]");

      Serial.println();

    }//End regular tag response
  }
  else
  {
    Serial.print("Unknown opcode: ");
    Serial.print(opCode);
    return (ERROR_UNKNOWN_OPCODE);
  }
*/
}

//Given an opcode, a piece of data, and the size of that data, package up a sentence and send it
void RFID::sendMessage(uint8_t opcode, uint8_t *data, uint8_t size, boolean waitForResponse)
{
  _nanoMsg[1] = size; //Load the length of this operation into msg array
  _nanoMsg[2] = opcode;

  //Copy the data into msg array
  for (uint8_t x = 0 ; x < size ; x++)
    _nanoMsg[3 + x] = data[x];

  sendCommand(waitForResponse); //Send and wait for response
}

//Given an array, calc CRC, assign header, send it out
//Modifies the caller's msg array
void RFID::sendCommand(boolean waitForResponse)
{
  _nanoMsg[0] = 0xFF; //Universal header
  uint8_t messageLength = _nanoMsg[1];
  uint8_t opcode = _nanoMsg[2]; //Used to see if response from module has the same opcode

  //Attach CRC
  uint16_t crc = calculateCRC(&_nanoMsg[1], messageLength + 2); //Calc CRC starting from spot 1, not 0. Add 2 for LEN and OPCODE bytes.
  _nanoMsg[messageLength + 3] = crc >> 8;
  _nanoMsg[messageLength + 4] = crc & 0xFF;

  Serial.print("sendCommand: ");
  for (uint8_t x = 0 ; x < messageLength + 5 ; x++)
  {
    Serial.print(" [");
    if (_nanoMsg[x] < 0x10) Serial.print("0");
    Serial.print(_nanoMsg[x], HEX);
    Serial.print("]");
  }
  Serial.println();

  //Remove anything in the incoming buffer
  //TODO this is a bad idea if we are constantly readings tags
  while (_nanoSerial->available()) _nanoSerial->read();

  //Send the command to the module
  for (uint8_t x = 0 ; x < messageLength + 5 ; x++)
    _nanoSerial->write(_nanoMsg[x]);

  //There are some commands (setBaud) that we can't or don't want the response
  if (waitForResponse == false) return;

  //For debugging, probably remove
  //for (uint8_t x = 0 ; x < 100 ; x++) _nanoMsg[x] = 0;

  //Wait for response with timeout
  long startTime = millis();
  while (_nanoSerial->available() == false)
  {
    if (millis() - startTime > COMMAND_TIME_OUT)
    {
      Serial.println("Time out-1");
      _nanoMsg[0] = ERROR_COMMAND_RESPONSE_TIMEOUT;
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
    if (millis() - startTime > COMMAND_TIME_OUT)
    {
      Serial.println("Time out-2");
      Serial.print("Fail :");
      Serial.println(spot);

      _nanoMsg[0] = ERROR_COMMAND_RESPONSE_TIMEOUT;
      return;
    }

    if (_nanoSerial->available())
    {
      _nanoMsg[spot] = _nanoSerial->read();

      if (spot == 1) //Grab the length of this response (spot 1)
        messageLength = _nanoMsg[1] + 7; //Actual length of response is ? + 7 for extra stuff (header, Length, opcode, 2 status bytes, ..., 2 bytes CRC = 7)

      spot++;

      //There's a case were we miss the end of one message and spill into another message.
      //We don't want spot pointing at an illegal spot in the array
      spot %= MAX_MSG_SIZE; //Wrap condition
    }
  }

  //Check CRC
  crc = calculateCRC(&_nanoMsg[1], messageLength - 3); //Remove header, remove 2 crc bytes
  if ((_nanoMsg[messageLength - 2] != (crc >> 8)) || (_nanoMsg[messageLength - 1] != (crc & 0xFF)))
  {
    _nanoMsg[0] = ERROR_CORRUPT_RESPONSE;
    Serial.println("Corrupt response");
    return;
  }

  //If crc is ok, check that opcode matches (did we get a response to the command we asked or a different one?)
  if (_nanoMsg[2] != opcode)
  {
    _nanoMsg[0] = ERROR_WRONG_OPCODE_RESPONSE;
    Serial.println("Wrong opcode response");
    return;
  }

  //If everything is ok, load all ok into msg array
  _nanoMsg[0] = ALL_GOOD;
}


/* Comes from serial_reader_l3.c
   ThingMagic-mutated CRC used for messages.
   Notably, not a CCITT CRC-16, though it looks close.
*/
static uint16_t crctable[] =
{
  0x0000, 0x1021, 0x2042, 0x3063,
  0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b,
  0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
};

//Calculates the magical CRC value
uint16_t RFID::calculateCRC(uint8_t *u8Buf, uint8_t len)
{
  uint16_t crc = 0xFFFF;

  for (uint8_t i = 0 ; i < len ; i++)
  {
    crc = ((crc << 4) | (u8Buf[i] >> 4)) ^ crctable[crc >> 12];
    crc = ((crc << 4) | (u8Buf[i] & 0x0F)) ^ crctable[crc >> 12];
  }

  return crc;
}
