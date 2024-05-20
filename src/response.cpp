#include "response.h"

Response::Response()
{
  memset(metadataOffsets, 0, TOTAL_METADATA);
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
  // assume that TOTAL_METADATA is at least 2
  metadataOffsets[0] = metadataLength; 
  metadataOffsets[1] = metadataLength; 
  for(uint8_t x = 0; x < TOTAL_METADATA; x++)
  {
    if(metadataFlag & unsigned(pow(2, x - 1)))
    { 
      metadataLength += (metadataLengths[x]);
    }    
    metadataOffsets[x] = metadataLength; 
  }
}

// resets the object
void Response::reset()
{
  headerLength = 0;
  metadataLength = 0;
  memset(metadataOffsets, 0, TOTAL_METADATA);
  nrTags = 0;
  metadataFlag = 0;
  opcode = 0;
  status = 0;
  temperature = 0;
}

// parses the header and puts it in the response
// also calculates metadata offsets and length
void Response::parse(uint8_t* msg, uint8_t length) 
{
  reset(); // reset object
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
    // change status
    status = (ERROR_CORRUPT_RESPONSE);
  }

  status = msg[i++] << 8 | msg[i++];

  // status is all good
  if(status == 0x0000)
  {
    // all read opcodes need to do similar actions
    if(opcode == TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE)
    {
      // temperature message
      if(msg[1] == 0x0B)
      {
        temperature = msg[15]; 
        status = RESPONSE_IS_TEMPERATURE;
      }
      // exlude other messages
      else if(msg[1] != 0x00 && msg[1] != 0x08)
      {
        // change status to tags found (substatus)
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
}


// merge the two data fields into one (untested...)
Response& Response::operator+(const Response &other)
{
  // only if there is data of the same sort
  if(other.nrTags > 0 && 
    other.opcode == this->opcode && 
    other.headerLength == this->headerLength &&
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
  if(tag < nrTags)
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
void Response::getMetadata(uint8_t tag, uint8_t *buf, uint16_t &bufLength)
{
  if(tag < nrTags)
  {
    uint16_t embeddedDataLength;
    uint8_t tagTypeLength;
    uint16_t tagPointer = getTagPointer(tag, embeddedDataLength, tagTypeLength);
    uint16_t length = metadataLength + embeddedDataLength + tagTypeLength;
    // make sure it does not exceed buffer length
    if(length > bufLength)
      length = bufLength;
    memcpy(buf, &(msg[tagPointer]), length);
    bufLength = length;
  }
}

// Converts metadata for tag into a json string by using the metadataFlag
uint16_t Response::metadataToJsonString(uint8_t tag, char *buf, int bufLength)
{
  if(tag < nrTags)
  {
    int length = 0;
    uint16_t embeddedDataLength;
    uint8_t tagTypeLength;
    uint16_t tagPointer = getTagPointer(tag, embeddedDataLength, tagTypeLength);
    length += snprintf(buf + length, bufLength - length, "{");
    for(uint8_t i = 1; i < TOTAL_METADATA; i++) 
    {
      // check if bit is on
      if(metadataFlag & unsigned(pow(2, i - 1)))
      {
        length += snprintf(buf + length, bufLength - length, "\"%s\": ", metadataLabels[i]);
        length += bytesToHexString(&(msg[tagPointer]), metadataLengths[i], buf + length, bufLength - length);  
        tagPointer += metadataLengths[i];
        length += snprintf(buf + length, bufLength - length, ", ");
        if(i == DATA)
        {
          length += snprintf(buf + length, bufLength - length, "\"Embedded Data\": ");
          length += bytesToHexString(&(msg[tagPointer]), embeddedDataLength, buf + length, bufLength - length);  
          tagPointer += embeddedDataLength;
          length += snprintf(buf + length, bufLength - length, ", ");
        }
        if(i == TAGTYPE)
        {
          // TODO
        }
      }
    }
    length += snprintf(buf + length, bufLength - length, "}");
    return length;
  }
  return 0;
}