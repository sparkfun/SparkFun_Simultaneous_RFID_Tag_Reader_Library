#include "response.h"

Response::Response()
{
  memset(metadataOffsets, 0, 15);
}

Response::Response(uint8_t *msg, uint8_t msgLength) 
{
  parse(msg, msgLength);
}

// calculates the metadata offsets for all fixed size members
// note individual metadata items can vary in size because of its dynamic members (tag type and embedded data)
void Response::calculateMetadataOffsets()
{
  metadataLength = 0;
  uint8_t i = 0;
  metadataOffsets[i++] = metadataLength; 
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, READCOUNT - 1)))
  { 
    metadataLength += (metadataLengths[READCOUNT]);
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, RSSI - 1)))
  {
    metadataLength += metadataLengths[RSSI];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, ANTENNAID - 1)))
  {
    metadataLength += metadataLengths[ANTENNAID];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, FREQUENCY - 1)))
  {
    metadataLength += metadataLengths[FREQUENCY];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, TIMESTAMP - 1)))
  {
    metadataLength += metadataLengths[TIMESTAMP];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, PHASE - 1)))
  {
    metadataLength += metadataLengths[PHASE];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, PROTOCOL - 1)))
  {
    metadataLength += metadataLengths[PROTOCOL];
  }
  metadataOffsets[i++] = metadataLength; 
  // note: more like datalength
  if(metadataFlag & unsigned(pow(2, DATA - 1)))
  {
    metadataLength += metadataLengths[DATA];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, GPIO_STATUS - 1)))
  {
    metadataLength += metadataLengths[GPIO_STATUS];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, GEN2_Q - 1)))
  {
    metadataLength += metadataLengths[GEN2_Q];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, GEN2_LF - 1)))
  {
    metadataLength += metadataLengths[GEN2_LF];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, GEN2_TARGET - 1)))
  {
    metadataLength += metadataLengths[GEN2_TARGET];
  }
  metadataOffsets[i++] = metadataLength; 
  if(metadataFlag & unsigned(pow(2, BRAND_IDENTIFIER - 1)))
  {
    metadataLength += metadataLengths[BRAND_IDENTIFIER];
  } 
  metadataOffsets[i++] = metadataLength; 
}

// parses the header and puts it in the response
// also calculates metadata offsets and length
void Response::parse(uint8_t* msg, uint8_t length) 
{
  // reset object
  headerLength = 0;
  metadataLength = 0;
  memset(metadataOffsets, 0, 15);
  nrTags = 0;
  metadataFlag = 0;
  opcode = 0;
  status = 0;
  // set new message
  msgLength = length;
  length = length > 512 ? 512 : length;
  for(uint8_t j = 0; j < length; j++)
    this->msg[j] = msg[j];
  uint8_t i = 1;
  msgLength = msg[i++] + 7; 
  opcode = msg[i++];

  // check crc
  uint16_t messageCRC = calculateCRC(&msg[1], msgLength - 3);
  if ((msg[msgLength - 2] != (messageCRC >> 8)) || (msg[msgLength - 1] != (messageCRC & 0xFF)))
  {
    status = (ERROR_CORRUPT_RESPONSE);
  }

  status = msg[i++] << 8 | msg[i++];

  // status is all good
  if(status == 0x0000)
  {
    status = RESPONSE_SUCCESS;
    // all read opcodes need to do similar actions
    if(opcode == TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE)
    {
      // exlude temperature and other messages
      if(msg[1] != 0x00 && msg[1] != 0x08 && msg[1] != 0x0a)
      {
        status = RESPONSE_IS_TAGFOUND;
        uint8_t multiselect = msg[i++];
        uint8_t optionByte = msg[i++];
        // check if there was metadata
        if(optionByte & 0x10)
        {
          i+=2; // skip search flags?
          metadataFlag = msg[i++] << 8 | msg[i++];
          nrTags = msg[i++]; // should be one in this case
          headerLength = i;
          calculateMetadataOffsets(); 
        }
      }
    }
    else if(opcode == TMR_SR_OPCODE_READ_TAG_DATA)
    {
      nrTags = 1;
      headerLength = i; // 3 bytes offset
      calculateMetadataOffsets();
    }
    else if(opcode == TMR_SR_OPCODE_GET_TAG_ID_BUFFER)
    {
      metadataFlag = msg[i++] << 8 | msg[i++];
      nrTags = msg[i++] << 8 | msg[i++];
      headerLength = i;
      calculateMetadataOffsets();
    }
  }
  else
  {
    status = (RESPONSE_FAIL);
  }
}


// merge the two data fields into one
Response& Response::operator+(const Response &other)
{
  // only if there is data of the same sort
  if(other.nrTags > 0 && 
    other.opcode == this->opcode && 
    other.status == this->status && 
    other.metadataFlag == this->metadataFlag &&
    other.msgLength + this->msgLength < 512)
  {
    // remove the header 
    for(uint8_t i = 0; i < other.msgLength; i++)
    {
      msg[i + this->msgLength] = other.msg[i];
    }
    this->msgLength += other.msgLength;
    this->nrTags += other.nrTags;
  }
  return *this;
}

// returns the index of the specified tag number (in case there are multiple)
// also returns the length of embedded data and tag type (if available)
uint16_t Response::getTagPointer(uint8_t tag, uint16_t &embeddedLength, uint8_t &tagTypeLength) 
{ 
  // default values
  embeddedLength = 0;
  tagTypeLength = 0;
  if(tag < nrTags)
  {
    uint16_t tagPointer = headerLength; // points to the start of the current item
    uint8_t offset = 0; // offset within item
    uint16_t embeddedDataLength, dataLength;
    for(uint8_t i = 0; i < tag; i++)
    {
      offset = 0;
      // these fields have a variable length
      if(unsigned(pow(2, DATA - 1)) & metadataFlag)
      {
        embeddedDataLength = msg[tagPointer + metadataOffsets[DATA]] << 8 | msg[tagPointer + metadataOffsets[DATA] + 1]; 
        embeddedDataLength = embeddedDataLength > 0 ? ((embeddedDataLength - 1) >> 3) + 1 : 0; // convert to bytes
        offset += embeddedDataLength;
      }
      if(unsigned(pow(2, TAGTYPE - 1)) & metadataFlag)
      {
        // TODO
      }
      // metadata + offset points to data segment
      tagPointer += (metadataLength + offset);
      // we assume dataLength is stored after metadata
      dataLength = msg[tagPointer] << 8 | msg[tagPointer + 1];
      dataLength = dataLength > 0 ? ((dataLength - 1) >> 3) + 1 : 0; // convert to bytes
      // data length + 2 points to next metadata field
      tagPointer += dataLength + 2;
    }
    // we have determined the start of the tag, now we need to determine the size of embedded data and tag type
    if(unsigned(pow(2, DATA - 1)) & metadataFlag)
    {
      embeddedLength = msg[tagPointer + metadataOffsets[DATA]] << 8 | msg[tagPointer + metadataOffsets[DATA] + 1]; 
      embeddedLength = embeddedLength > 0 ? ((embeddedLength - 1) >> 3) + 1 : 0; // convert to bytes
    }
    if(unsigned(pow(2, TAGTYPE - 1)) & metadataFlag)
    {
      // TODO
    }
    return tagPointer;
  }
  return 0;
}

// we assume the bufLength is set to the buffer size
// if epclength < buflength, we change buflength
void Response::getData(uint8_t tag, uint8_t *buf, uint16_t &bufLength, uint8_t start)
{
  uint16_t dataLength;
  uint16_t dataPointer;
  if(opcode == TMR_SR_OPCODE_READ_TAG_DATA)
  {   
    dataPointer = headerLength + 1;
  }
  else
  {
    uint16_t embeddedDataLength; 
    uint8_t tagTypeLength;
    uint16_t tagPointer = getTagPointer(tag, embeddedDataLength, tagTypeLength);
    // data is stored at the base + length of embedded and tag type + all metadata offsets
    dataPointer = tagPointer + metadataOffsets[TAGTYPE] + embeddedDataLength + tagTypeLength;
  }
  if(msgLength > dataPointer + 4 + start)
  {
    dataLength = msgLength - dataPointer - 4;
    if(dataLength > bufLength)
      dataLength = bufLength;
    memcpy(buf, &(msg[start + dataPointer]), dataLength);
    bufLength = dataLength;
  }
  // could not read any bytes
  else
  {
    bufLength = 0;
  }
}

// helper function that returns from start addres 2 (default includes length)
void Response::getBankdata(uint8_t tag, uint8_t *buf, uint16_t &bufLength)
{
  getData(tag, buf, bufLength, 0);
}

// helper function that returns from start addres 4
void Response::getEPCdata(uint8_t tag, uint8_t *buf, uint16_t &bufLength)
{
  getData(tag, buf, bufLength, 4);
}

// returns all metadata for a tag
void Response::getMetadata(uint8_t tag, uint8_t *buf, uint16_t &bufLength, uint16_t &embeddedDataLength, uint8_t &tagTypeLength)
{
  uint16_t tagPointer = getTagPointer(tag, embeddedDataLength, tagTypeLength);
  uint16_t length = metadataLength + embeddedDataLength + tagTypeLength;
  // make sure it does not exceed buffer length
  if(length > bufLength)
    length = bufLength;
  memcpy(buf, &(msg[tagPointer]), length);
  bufLength = length;
}

// Prints the metadata for tag
// Could not think of an easier way...
void Response::printMetadata(uint8_t tag)
{
  uint16_t embeddedDataLength;
  uint8_t tagTypeLength;
  uint16_t tagPointer = getTagPointer(tag, embeddedDataLength, tagTypeLength);
  if(metadataFlag & unsigned(pow(2, READCOUNT - 1)))
  {
    Serial.print("Readcount: ");
    Serial.println(msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, RSSI - 1)))
  {
    Serial.print("RSSI: ");
    Serial.println(msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, ANTENNAID - 1)))
  {
    Serial.print("Antenna ID: ");
    Serial.println(msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, FREQUENCY - 1)))
  {
    Serial.print("Frequency: ");
    Serial.println(msg[tagPointer++] << 16 | msg[tagPointer++] << 8 | msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, TIMESTAMP - 1)))
  {
    Serial.print("Timestamp: ");
    Serial.println(msg[tagPointer++] << 24 | msg[tagPointer++] << 16 | msg[tagPointer++] << 8 | msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, PHASE - 1)))
  {
    Serial.print("Phase: ");
    Serial.println(msg[tagPointer++] << 8 | msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, PROTOCOL - 1)))
  {
    Serial.print("Protocol: ");
    Serial.println(msg[tagPointer++]);
  }
  // note: more like datalength
  if(metadataFlag & unsigned(pow(2, DATA - 1)))
  {
    Serial.print("Embedded Data Length: ");
    Serial.println(embeddedDataLength);
    tagPointer += 2;
    Serial.print("Embedded Data:");
    printBytes(&(msg[tagPointer]), embeddedDataLength);
    tagPointer += embeddedDataLength;
  }
  if(metadataFlag & unsigned(pow(2, GPIO_STATUS - 1)))
  {
    Serial.print("GPIO: ");
    Serial.println(msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, GEN2_Q - 1)))
  {
    Serial.print("Gen2 Q: ");
    Serial.println(msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, GEN2_LF - 1)))
  {
    Serial.print("Gen2 LF: ");
    Serial.println(msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, GEN2_TARGET - 1)))
  {
    Serial.print("Gen2 Target: ");
    Serial.println(msg[tagPointer++]);
  }
  if(metadataFlag & unsigned(pow(2, BRAND_IDENTIFIER - 1)))
  {
    Serial.print("Brand Identifier: ");
    Serial.println(msg[tagPointer++] << 8 | msg[tagPointer++]);
  } 
  if(metadataFlag & unsigned(pow(2, TAGTYPE - 1)))
  {
    Serial.print("Tag Type: ");
    printBytes(&(msg[tagPointer]), tagTypeLength);
  } 
}
