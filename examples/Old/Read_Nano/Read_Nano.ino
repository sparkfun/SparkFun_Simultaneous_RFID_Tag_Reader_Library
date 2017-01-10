#include <osdep.h>
#include <serial_reader_imp.h>
#include <tm_config.h>
#include <tm_reader.h>
#include <tmr_filter.h>
#include <tmr_gen2.h>
#include <tmr_gpio.h>
#include <tmr_ipx.h>
#include <tmr_iso180006b.h>
#include <tmr_params.h>
#include <tmr_read_plan.h>
#include <tmr_region.h>
#include <tmr_serial_reader.h>
#include <tmr_serial_transport.h>
#include <tmr_status.h>
#include <tmr_tag_auth.h>
#include <tmr_tag_data.h>
#include <tmr_tag_lock_action.h>
#include <tmr_tag_protocol.h>
#include <tmr_tagop.h>
#include <tmr_types.h>
#include <tmr_utils.h>

#define ERROR_BLINK_COUNT 10
#define ERROR_BLINK_INTERVAL 100
#define LOG_SERIAL_COMM 0
#define TAG_SEARCH_TIME 250
#define SEARCH_INTERVAL 1000 //Search for new batch of tags every 1000ms
#define CONSOLE_BAUD_RATE 115200
#define TM_BAUD_RATE 115200
#define SERIAL_PROXY_BAUD_RATE 115200
#define SERIAL_PROXY_COM1 (&Serial1)

const int sysLedPin = 13;
static int ledState = LOW; // ledState used to set the LED
static HardwareSerial *console = &Serial;

const int tmGpio1Pin = 10;
const int tmGpio2Pin = 9;
const int tmGpio3Pin = 8;
const int tmGpio4Pin = 7;

static TMR_Reader r, *rp;
static TMR_TransportListenerBlock tb;

static HardwareSerial *readerModule = &Serial1;

static unsigned long nextTagSearchTime = 0;

static void blink(int count, int blinkInterval)
{
  unsigned long blinkTime;
  unsigned long currentTime;
  blinkTime = 0;
  currentTime = millis();
  while (count)
  {
    if (currentTime > blinkTime) {
      // save the last time you blinked the LED
      blinkTime = currentTime + blinkInterval;

      // if the LED is off turn it on and viceversa:
      if (ledState == LOW)
      {
        ledState = HIGH;
      }
      else
      {
        ledState = LOW;
        count;
      }
      // set the LED with the ledState of the variable:
      digitalWrite(sysLedPin, ledState);
    }
    currentTime = millis();
  }
}

static void checkerr(TMR_Reader* rp, TMR_Status ret, int exitval, const char *msg)
{
  if (TMR_SUCCESS != ret)
  {
    while ( 1 )
    {
      console->print("ERROR ");
      console->print(msg);
      console->print(": 0x");
      console->print(ret, HEX);
      console->print(": ");
      // console->println(TMR_strerror(ret));
      blink(ERROR_BLINK_COUNT, ERROR_BLINK_INTERVAL);
    }
  }
}

static void printComment(char *msg)
{
  console->print("#");
  console->println(msg);
}

static void initializeReader()
{
  TMR_Status ret;
  rp = &r;
  ret = TMR_create(rp, "tmr:///Serial1");
  checkerr(rp, ret, 1, "creating reader");
  {
    uint32_t rate;
    rate = TM_BAUD_RATE;
    ret = rp->paramSet(rp, TMR_PARAM_BAUDRATE, &rate);
  }
  // Set region to North America
  {
    TMR_Region region;
    region = TMR_REGION_NA3;
    ret = TMR_paramSet(rp, TMR_PARAM_REGION_ID, &region);
    checkerr(rp, ret, 1, "setting region");
  }
  ret = TMR_connect(rp);
  checkerr(rp, ret, 1, "connecting reader");
  // The number of tags in the field at once is expected to be few, and we want to see those tags as much as possible
  // So set the Session to 0
  {
    TMR_GEN2_Session value;
    value = TMR_GEN2_SESSION_S0;
    ret = TMR_paramSet(rp, TMR_PARAM_GEN2_SESSION, &value);
    checkerr(rp, ret, 1, "setting session");
  }
  // set the reader to toggle targets
  {
    TMR_GEN2_Target value;
    value = TMR_GEN2_TARGET_AB;
    ret = TMR_paramSet(rp, TMR_PARAM_GEN2_TARGET, &value);
    checkerr(rp, ret, 1, "setting target AB");
  }
}

static void reportTags()
{
  TMR_Status ret;
  while (TMR_SUCCESS == TMR_hasMoreTags(rp))
  {
    TMR_TagReadData trd;
    char epcStr[128];
    // blink(20,300);

    console->print("\n");

    ret = TMR_getNextTag(rp, &trd);
    checkerr(rp, ret, 1, "fetching tag");
    TMR_bytesToHex(trd.tag.epc, trd.tag.epcByteCount, epcStr);

    console->print("TAGREAD EPC:");
    console->print(epcStr);

    if (TMR_TRD_METADATA_FLAG_PROTOCOL & trd.metadataFlags)
    {
      console->print(" PROT:");
      console->print(trd.tag.protocol);
    }
    if (TMR_TRD_METADATA_FLAG_ANTENNAID & trd.metadataFlags)
    {
      console->print(" ANT:");
      console->print(trd.antenna);
    }
    if (TMR_TRD_METADATA_FLAG_READCOUNT & trd.metadataFlags)
    {
      console->print(" READCOUNT:");
      console->print(trd.readCount);
    }
    if (TMR_TRD_METADATA_FLAG_RSSI & trd.metadataFlags)
    {
      console->print(" RSSI:");
      console->print(trd.rssi);
    }
    if (TMR_TRD_METADATA_FLAG_FREQUENCY & trd.metadataFlags)
    {
      console->print(" FREQUENCY:");
      console->print(trd.frequency);
    }
    // if (TMR_TRD_METADATA_FLAG_TIMESTAMP & trd.metadataFlags)
    // {
    // console->print(" TIMESTAMP:");
    // console->print(trd.dspMicros);
    // }
    if (TMR_TRD_METADATA_FLAG_PHASE & trd.metadataFlags)
    {
      console->print(" PHASE:");
      console->print(trd.phase);
    }
  }
}

static void readTags()
{
  TMR_Status ret;
  TMR_ReadPlan plan;

  uint8_t antennaCount = 0x1;
  uint8_t antennaList[1] = {1};

  ret = TMR_RP_init_simple(&plan, antennaCount, &antennaList[0], TMR_TAG_PROTOCOL_GEN2, 1000);

  checkerr(rp, ret, 1, "initializing the read plan");

  /* Commit read plan */

  ret = TMR_paramSet(rp, TMR_PARAM_READ_PLAN, &plan);

  checkerr(rp, ret, 1, "setting read plan");

  ret = TMR_read(rp, TAG_SEARCH_TIME, NULL);

  checkerr(rp, ret, 1, "reading tags");
  
  reportTags();
}

void setup()
{
  // Set the GPIO pins that we are concerned about to output
  pinMode(sysLedPin, OUTPUT);

  // initialize the GPIO pins
  digitalWrite(sysLedPin, 0);

  // start the console interface
  console->begin(CONSOLE_BAUD_RATE);

  initializeReader();

  console->print("\n");
}

void loop()
{
  unsigned long currentTime;
  currentTime = millis();

  if (currentTime >= nextTagSearchTime)
  {
    // save the last time a search was performed
    nextTagSearchTime = currentTime + SEARCH_INTERVAL;
    readTags();
    currentTime = millis();
  }
}
