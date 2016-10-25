/*
  Read any local tags
  Nathan Seidle @ SparkFun Electronics
  October 3rd, 2016



*/

#include <stdio.h>
#include <string.h>

#include <SimpleTimer.h> //https://github.com/jfturcot/SimpleTimer
SimpleTimer timer; //Used to blink LED and other time based things

#define STAT 13 //Status LED on most Arduino boards

//Global variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
long secondTimerID;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void setup()
{
  pinMode(STAT, OUTPUT);
  //secondTimerID = timer.setInterval(1000L, secondPassed); //Call secondPassed every Xms

  Serial.begin(115200);
  Serial.println("testing");

  //FF  00  03  1D  0C
  //byte test[2] = {0x00, 0x03};
  //byte test[2] = {0xAB, 0x12};

  byte test[4] = {0x02, 0x93, 0x00, 0x05};

  //14  03  00  00  14  12  08  00  30  00  00  02  20  15  03 10  01  03  01  47  00  00  00  10
  //byte test[24] = {0x14, 0x03, 0x00, 0x00, 0x14, 0x12, 0x08, 0x00, 0x30, 0x00, 0x00, 0x02, 0x20, 0x15, 0x03, 0x10, 0x01, 0x03, 0x01, 0x47, 0x00, 0x00, 0x00, 0x10};

  sendCommand(test, sizeof(test));
}

void loop()
{
  timer.run(); //Update any timers we are running
  delay(1);
}

//Sends a command to the RFID module
//Adds header and checksum
bool sendCommand(byte *thingToSend, byte len)
{
  uint16_t crc = calculateCRC(thingToSend, len); //Calc CRC for this array

  //Serial.print("CRC: 0x");
  //Serial.println(crc, HEX);

  byte test[len + 3]; //Create new array that will hold header and CRC (2 bytes)

  test[0] = 0xFF; //Attach header

  //Add what the caller wants
  for (byte x = 0 ; x < len ; x++)
    test[x + 1] = thingToSend[x];

  //Append the CRC onto our array
  test[len + 1] = crc >> 8;
  test[len + 2] = crc & 0xFF;

  //Send array to module
  Serial.print("sendCommand: ");
  printHexString(test, sizeof(test));
  Serial.println();
}

void printHexString(byte *thing, byte len)
{
  for (int x = 0 ; x < len ; x++)
  {
    //Serial.print("0x");
    if (thing[x] < 0x0F) Serial.print("0");
    Serial.print(thing[x], HEX);
    Serial.print(" ");
  }
}

/* 
   From serial_reader_l3.c of API
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

//Given a string from the device (sans 0xFF header), and length (minus 2 to remove CRC bytes)
//Returns a calculated CRC that you can then test against
uint16_t calculateCRC(uint8_t *u8Buf, uint8_t len)
{
  uint16_t crc;
  int i;

  crc = 0xffff;

  for (i = 0; i < len ; i++)
  {
    crc = ((crc << 4) | (u8Buf[i] >> 4))  ^ crctable[crc >> 12];
    crc = ((crc << 4) | (u8Buf[i] & 0xf)) ^ crctable[crc >> 12];
  }

  return crc;
}


//Given a string from the RFID module, check the checksum
//Returns true if checksum is good
bool verifyChecksum(String incomingString)
{
  //Checksum works like this:
  //If we are sending (Host-to-Reader Communication):
  //  FF  00  0C  1D  03
  //Header, length, command, data (0), CRC
  //If we are receiving:
  //  FF  01  0C  00  00  52  63  03

  //Add all the doublets together and remove any leading digits more than two from the total
  //For example: 16 24 11 15 34 15 38 15 25 = 193 = 93
  //193 becomes 93

  int sentenceLength = incomingString.length();

  //Peel off two characters, turn them into a 2 digit int, add all them together excluding
  //the checksum value doublet at the end of the string
  int sentenceChecksum = 0;
  for (int spot = 0 ; spot < sentenceLength - 3 ; spot += 2)
    sentenceChecksum += incomingString.substring(spot, spot + 2).toInt();

  sentenceChecksum %= 100; //Get to a two digit number

  //Convert the 2 digits at the end of the sentence to a checksum value
  int incomingChecksum = incomingString.substring(sentenceLength - 2, sentenceLength).toInt();

  if (incomingChecksum == sentenceChecksum) return (true);
  else return (false);
}

/*
  //Sends a command to the RFID module
  //Adds header, data length, and checksum
  bool sendCommand(byte thingToSend)
  {
  String strThingToSend = (String)thingToSend; //Convert the command number to string
  while (strThingToSend.length() < 2) strThingToSend = '0' + strThingToSend; //Add the leading zero get to two characters

  //Calculate the leader: length of the string we need to send
  //Leader is a count of all the doublets in the string including the checksum
  int lengthOfThing = (strThingToSend.length() + 2) / 2; //Add two for the length of the checksum

  //Create the leader, 4 characters
  String strLeader = String(lengthOfThing);
  while (strLeader.length() < 4) strLeader = '0' + strLeader; //Add the leading zero get to four characters

  //Tack the leader on to the front
  strThingToSend = strLeader + strThingToSend;

  //Calculate checksum for this string
  int checksum = 0;
  for (int x = 0 ; x < strThingToSend.length() ; x += 2)
    checksum += strThingToSend.substring(x, x + 2).toInt();
  checksum %= 100; //Reduce to two digits

  //Checksum must be two characters in length
  String strCheckSum = String(checksum);
  while (strCheckSum.length() < 2) strCheckSum = '0' + strCheckSum; //Add the leading zero to checksum if less than ten

  strThingToSend = '$' + strThingToSend + strCheckSum + '&'; //Stick this checksum on the end, surround with $ and &

  laserSerial.print(strThingToSend); //Send it out

  String response = getResponse(MAX_COMMAND_WAIT);

  //#ifdef debugSerial
  //debugSerial.print("ACK response: ");
  //debugSerial.println(response);
  //#endif

  //Check checksum to make sure we didn't miss any characters
  if (verifyChecksum(response) == false) return (false);

  //#ifdef debugSerial
  //debugSerial.println("ACK checksum valid");
  //#endif

  LaserRecord laserData = crackLaserSentence(response); //Crack this response into the various bits of data (record type, reading number, max, min, etc)

  if (laserData.type == COMMAND_ACK) return (true);

  return (false);
  }

  //Given a String response from the laser, cracks it into the various sub bits
  //If the response is a distance reading, returns distance
  //If response is an ACK, returns ACK
  //If record type is unknown, returns record type
  LaserRecord crackLaserSentence(String laserResponse)
  {
  LaserRecord record;

  //Pull out record length
  record.length = laserResponse.substring(0, 4).toInt();

  //Pull out the record type
  record.type = laserResponse.substring(4, 6).toInt();

  //#ifdef debugSerial
  //debugSerial.print("laserResponse: ");
  //debugSerial.println(laserResponse);
  //debugSerial.print("length: ");
  //debugSerial.println(record.length);
  //debugSerial.print("type: ");
  //debugSerial.println(record.type);
  //#endif

  if (record.type == COMMAND_MULTI_READ) //This is the multi-reading type record
  {
    //Example sentences:
    //001624 0085 000 00098 000 01081 000 00097 = record 85, 00.098m current, 01.081m max, 00.097m min
    //012345 6789 012 34567 890 12345 678 90123

    //001624 0011 000 01078 000 01078 000 01077 = record 11, 01.078m current, 01.078m max, 01.077m min
    //012345 6789 012 34567 890 12345 678 90123

    record.reading = laserResponse.substring(6, 10).toInt();
    record.currentDistance = laserResponse.substring(13, 18).toInt();
    record.maxDistance = laserResponse.substring(21, 26).toInt();
    record.minDistance = laserResponse.substring(29, 34).toInt();
    record.time = millis();  //Timestamp this reading
  }
  else if (record.type == COMMAND_SINGLE_SHOT) //This is the single shot record
  {
    //Example sentence:
    //0006 21 0000 1235 74 - Distance is 00001.235m
    //0123 45 6789 0123 45 - Character locations
    record.currentDistance = laserResponse.substring(6, 14).toInt();
    record.time = millis();  //Timestamp this reading
  }
  else if (record.type == COMMAND_ACK) //Respond ACK
  {
  }
  else
  {
    //Serial.print(" Record type: ");
    //Serial.println(record.type);
  }

  return (record);

  }

*/

