# **Problem**

The [SparkFun Simultaneous RFID Tag Reader Library](https://github.com/sparkfun/SparkFun_Simultaneous_RFID_Tag_Reader_Library) has several limitations.
The main limitation is the lack of support for read and write **filters**.
Also, the read functionality only reads **one tag at a time**, except for the continuous mode.
Continuous mode can however not be **customized**.
Lastly, it does not use the most **optimal** opcodes for reading and writing.

For reading it uses the _READ_TAG_DATA_ opcode whilst _READ_TAG_ID_MULTIPLE_ or _READ_TAG_ID_SINGLE_ would be more efficient.

The same applies to writing: it always uses _WRITE_TAG_DATA_, where _WRITE_TAG_ID_ would be more efficient and secure for writing IDs.

Overall the library code is functional, but far from complete.

# **Requirements**

For our purposes, we need the following:

1. **Write filters**: without a write filter we might select the wrong tag to write to, if more than one are being detected
2. **Read filters:** to read the complete content of a tag.
3. **Multi-tag single read:** in path authentication we need to change between reading and writing, so continuous read is not possible. So, we need a single read that detects all tags in the proximity.

So, the main thing we need to implement is filter support. We also need to implement a new read function for multi-tag single read.

Additionally, we will use the proper opcodes, meaning support for _READ_TAG_ID_MULTIPLE_ and _WRITE_TAG_ID_.
# **Approach**
Our approach has two steps. First we go through the code for the MercuryAPI v1.37.2.24.
After that we obtained some communication samples using the Universal Reader Assistant v6.2. We connected the reader directly to USB using the CH340 connector.
The code gives a more definite answer about the structure, whilst the captured samples give us some info about which values to use.
# **Opcodes**
The opcodes are defined in the TMR_SR_OpCode enum in serial_reader_imp.h:39.
Most implementations of these commands can be found in serial_reader_l3.c.
We are interested in the following opcodes:

|Opcode|Mnemonic|Function in MercuryAPI|
|---|---|---|
|0x22|READ_TAG_ID_MULTIPLE|TMR_SR_cmdReadTagMultiple (line 1476)|
|0x23|WRITE_TAG_ID|TMR_SR_cmdWriteGen2TagEpc (line 1820)|
|0x24|WRITE_TAG_DATA|TMR_SR_cmdWriteMemory (line 10041)<br><br>TMR_SR_cmdGEN2WriteTagData (line 2629)<br><br>TMR_SR_cmdISO180006BWriteTagData (line 2545)|
|0x28|READ_TAG_DATA|TMR_SR_cmdReadMemory (line 10146)<br><br>TMR_SR_cmdGEN2ReadTagData (line 2829)<br><br>TMR_SR_cmdISO180006BReadTagData (line 2491)|
|0x29|GET_TAG_ID_BUFFER|TMR_SR_cmdGetTagsRemaining (line 1978)|
|0x2F|MULTI_PROTOCOL_TAG_OP|TMR_SR_msgSetupMultipleProtocolSearch (line 4612)|

By the look of it, _WRITE_TAG_ID_ only works with Gen2 tags.
This is the go to standard so we will implement it anyways.
In _READ_TAG_DATA_ and _WRITE_TAG_DATA_ there are differences between ISO-18000B, Gen2 and generic. We will focus on the generic first.
Additionally, we focus on the code for filters, presented in the _filterbytes_ function, line 4923.

# **Understanding the Code**

The M6E will never initiate communication, so it is always responding to the host.
The documentation does not specify the structure of individual opcodes, so that is why we need to go through the code.
The list of opcodes is given in the TMR_SR_OpCode enum in serial_reader_imp.h:

```cpp fold title:"Opcodes"
typedef enum TMR_SR_OpCode  
{  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_WRITE_FLASH             = 0x01,  
  TMR_SR_OPCODE_READ_FLASH              = 0x02,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_VERSION                 = 0x03,  
  TMR_SR_OPCODE_BOOT_FIRMWARE           = 0x04,  
  TMR_SR_OPCODE_SET_BAUD_RATE           = 0x06,  
  TMR_SR_OPCODE_ERASE_FLASH             = 0x07,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_VERIFY_IMAGE_CRC        = 0x08,  
#endif  /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_BOOT_BOOTLOADER         = 0x09,  
  TMR_SR_OPCODE_HW_VERSION              = 0x10,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_MODIFY_FLASH            = 0x0A,  
  TMR_SR_OPCODE_GET_DSP_SILICON_ID      = 0x0B,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_GET_CURRENT_PROGRAM     = 0x0C,  
  TMR_SR_OPCODE_WRITE_FLASH_SECTOR      = 0x0D,  
  TMR_SR_OPCODE_GET_SECTOR_SIZE         = 0x0E,  
  TMR_SR_OPCODE_MODIFY_FLASH_SECTOR     = 0x0F,  
  TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE    = 0x22,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_WRITE_TAG_ID            = 0x23,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_WRITE_TAG_DATA          = 0x24,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_LOCK_TAG                = 0x25,  
  TMR_SR_OPCODE_KILL_TAG                = 0x26,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_READ_TAG_DATA           = 0x28,  
  TMR_SR_OPCODE_PASS_THROUGH            = 0x27,  
  TMR_SR_OPCODE_GET_TAG_ID_BUFFER       = 0x29,  
  TMR_SR_OPCODE_CLEAR_TAG_ID_BUFFER     = 0x2A,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_WRITE_TAG_SPECIFIC      = 0x2D,  
  TMR_SR_OPCODE_ERASE_BLOCK_TAG_SPECIFIC= 0x2E,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP   = 0x2F,  
  TMR_SR_OPCODE_GET_ANTENNA_PORT        = 0x61,  
  TMR_SR_OPCODE_GET_READ_TX_POWER       = 0x62,  
  TMR_SR_OPCODE_GET_TAG_PROTOCOL        = 0x63,  
  TMR_SR_OPCODE_GET_WRITE_TX_POWER      = 0x64,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_GET_FREQ_HOP_TABLE      = 0x65,  
#endif /* TMR_ENABLE_UHF*/  
  TMR_SR_OPCODE_GET_USER_GPIO_INPUTS    = 0x66,  
  TMR_SR_OPCODE_GET_REGION              = 0x67,  
  TMR_SR_OPCODE_GET_POWER_MODE          = 0x68,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_GET_USER_MODE           = 0x69,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_GET_READER_OPTIONAL_PARAMS=0x6A,  
  TMR_SR_OPCODE_GET_PROTOCOL_PARAM      = 0x6B,  
  TMR_SR_OPCODE_GET_READER_STATS        = 0x6C,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_GET_USER_PROFILE        = 0x6D,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_GET_AVAILABLE_PROTOCOLS = 0x70,  
  TMR_SR_OPCODE_GET_AVAILABLE_REGIONS   = 0x71,  
  TMR_SR_OPCODE_GET_TEMPERATURE         = 0x72,  
  TMR_SR_OPCODE_SET_ANTENNA_PORT        = 0x91,  
  TMR_SR_OPCODE_SET_READ_TX_POWER       = 0x92,  
  TMR_SR_OPCODE_SET_TAG_PROTOCOL        = 0x93,  
  TMR_SR_OPCODE_SET_WRITE_TX_POWER      = 0x94,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_SET_FREQ_HOP_TABLE      = 0x95,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_SET_USER_GPIO_OUTPUTS   = 0x96,  
  TMR_SR_OPCODE_SET_REGION              = 0x97,  
  TMR_SR_OPCODE_SET_POWER_MODE          = 0x98,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_SET_USER_MODE           =  0x99,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_OPCODE_SET_READER_OPTIONAL_PARAMS=0x9a,  
  TMR_SR_OPCODE_SET_PROTOCOL_PARAM      = 0x9B,  
  TMR_SR_OPCODE_SET_USER_PROFILE        = 0x9D,  
  TMR_SR_OPCODE_SET_PROTOCOL_LICENSEKEY = 0x9E,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_OPCODE_SET_OPERATING_FREQ      = 0xC1,  
  TMR_SR_OPCODE_TX_CW_SIGNAL            = 0xC3,  
#endif /* TMR_ENABLE_UHF */  
}TMR_SR_OpCode;
```

Each of these opcodes has a different request and possibly a different response.
Some of the opcodes are highly customizeable.
Some common options are including metadata or not, if the reader is in continuous mode, or if the reader has enabled multi-select.
We will go through the code per opcode.
We do this by first describing the code for sending requests and responses.
The second section will show the communication protocol for the opcode and cross reference it with an actual message from URA.
After that, we show an implementation of this opcode in the SparkFun library.
## **Filter**
The filter is an important component in most opcodes so we treat it on its own.
### **Code For Constructing Filter**

The following code is used to construct a filter.

```cpp fold title:"Filter Construction"
static TMR_Status  
filterbytes(TMR_TagProtocol protocol, const TMR_TagFilter *filter,   
            uint8_t *option, uint8_t *i, uint8_t *msg,  
            uint32_t accessPassword, bool usePassword)  
{  
#ifdef TMR_ENABLE_UHF  
  int j;  
  if (isSecureAccessEnabled)  
  {  
    *option = TMR_SR_GEN2_SINGULATION_OPTION_SECURE_READ_DATA;  
  }  
#endif /* TMR_ENABLE_UHF */  
  
  if (NULL == filter && 0 == accessPassword)  
  {  
    *option |= 0x00;  
    return TMR_SUCCESS;  
  }  
  
#ifdef TMR_ENABLE_UHF  
  if (TMR_TAG_PROTOCOL_GEN2 == protocol)  
  {  
    if (usePassword)  
    {  
      if ((NULL != filter) &&  
          (TMR_FILTER_TYPE_GEN2_SELECT == filter->type) &&  
          (TMR_GEN2_EPC_LENGTH_FILTER == filter->u.gen2Select.bank))  
      {  
        /* Ignore access password */  
      }  
      else  
      {  
        SETU32(msg, *i, accessPassword);  
      }  
    }  
  
    if (NULL == filter)  
    {  
      *option |= TMR_SR_GEN2_SINGULATION_OPTION_USE_PASSWORD;  
      if(isMultiSelectEnabled)  
      {  
        SETU8(msg, *i, SELECT);  
        SETU8(msg, *i, ON_N_OFF);  
        SETU8(msg, *i, TMR_SR_END_OF_SELECT_INDICATOR);  
      }  
    }  
    else if (TMR_FILTER_TYPE_GEN2_SELECT == filter->type)  
    {  
      const TMR_GEN2_Select *fp;  
  
      fp = &filter->u.gen2Select;  
  
      if (1 == fp->bank)  
      {  
        *option |= TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_ADDRESSED_EPC;  
      }  
      else /* select based on the bank */  
      {  
        *option |= fp->bank;  
      }  
  
	  if (fp->bank == TMR_GEN2_EPC_LENGTH_FILTER)  
	  {  
		  SETU16(msg, *i, fp->maskBitLength);  
	  }  
	  else  
	  {  
		  if(true == fp->invert)  
		  {  
			*option |= TMR_SR_GEN2_SINGULATION_OPTION_INVERSE_SELECT_BIT;  
		  }  
  
		  if (fp->maskBitLength > 255)  
		  {  
			*option |= TMR_SR_GEN2_SINGULATION_OPTION_EXTENDED_DATA_LENGTH;  
		  }  
  
		  SETU32(msg, *i, fp->bitPointer);  
  
		  if (fp->maskBitLength > 255)  
		  {  
			SETU8(msg, *i, (fp->maskBitLength >> 8) & 0xFF);  
		  }  
		  SETU8(msg, *i, fp->maskBitLength & 0xFF);  
  
		  for(j = 0; j < tm_u8s_per_bits(fp->maskBitLength) ; j++)  
		  {  
			SETU8(msg, *i, fp->mask[j]);  
		  }  
      }  
      if(isMultiSelectEnabled)  
      {  
        SETU8(msg, *i, filter->u.gen2Select.target);  
        SETU8(msg, *i, filter->u.gen2Select.action);  
        SETU8(msg, *i, TMR_SR_END_OF_SELECT_INDICATOR);  
      }  
    }  
    else if (TMR_FILTER_TYPE_TAG_DATA == filter->type)  
    {  
      const TMR_TagData *fp;  
      int bitCount;  
  
      fp = &filter->u.tagData;  
      bitCount = fp->epcByteCount * 8;  
  
      /* select on the EPC */  
      *option |= 1;  
      if (bitCount > 255)  
      {  
        *option |= TMR_SR_GEN2_SINGULATION_OPTION_EXTENDED_DATA_LENGTH;  
        SETU8(msg, *i, (bitCount>>8) & 0xFF);  
      }  
      SETU8(msg, *i, (bitCount & 0xFF));  
  
      for(j = 0 ; j < fp->epcByteCount ; j++)  
      {  
        SETU8(msg, *i, fp->epc[j]);  
      }  
    }  
    else if(TMR_FILTER_TYPE_MULTI == filter->type)  
    {  
      int j;  
      uint16_t filterLength, temp;  
      filterLength = filter->u.multiFilterList.len;  
  
      if(!isMultiSelectEnabled)  
      {  
        return TMR_ERROR_UNSUPPORTED;  
      }  
  
      for(j = 0; j < filterLength; j++)  
      {  
        if(filter->u.multiFilterList.tagFilterList[j]->type == TMR_FILTER_TYPE_TAG_DATA)  
        {  
          return TMR_ERROR_UNSUPPORTED;  
        }  
      }  
  
      if(filterLength > NUMBER_OF_MULTISELECT_SUPPORTED)  
      {  
        return TMR_ERROR_UNSUPPORTED;  
      }  
      else  
      {  
        temp = 0;  
        filterbytes(protocol, filter->u.multiFilterList.tagFilterList[temp++], option, i, msg, accessPassword, false);  
  
        while (temp < filterLength)  
        {  
          option = &msg[(*i) - 1];  
          filterbytes(protocol, filter->u.multiFilterList.tagFilterList[temp++], option, i, msg, accessPassword, false);  
        }  
      }  
    }  
    else  
    {  
      return TMR_ERROR_INVALID;  
    }  
  }  
#ifdef TMR_ENABLE_ISO180006B  
  else if (TMR_TAG_PROTOCOL_ISO180006B == protocol)  
  {  
    if (option)  
    {  
      *option = 1;  
    }  
      
    if (NULL == filter)  
    {  
       /* Set up a match-anything filter, since it isn't the default */  
      SETU8(msg, *i, TMR_ISO180006B_SELECT_OP_EQUALS);  
      SETU8(msg, *i, 0); /* address */  
      SETU8(msg, *i, 0); /* mask - don't compare anything */  
      SETU32(msg, *i, 0); /* dummy tag ID bytes 0-3, not compared */  
      SETU32(msg, *i, 0); /* dummy tag ID bytes 4-7, not compared */  
      return TMR_SUCCESS;  
    }  
    else if (TMR_FILTER_TYPE_ISO180006B_SELECT == filter->type)  
    {  
      const TMR_ISO180006B_Select *fp;  
        
      fp = &filter->u.iso180006bSelect;  
      if (false == fp->invert)  
      {  
        SETU8(msg, *i, fp->op);  
      }  
      else  
      {  
        SETU8(msg, *i, fp->op | 4);  
      }  
      SETU8(msg, *i, fp->address);  
      SETU8(msg, *i, fp->mask);  
      for (j = 0 ; j < 8 ; j++)  
      {  
        SETU8(msg, *i, fp->data[j]);  
      }  
    }  
    else if (TMR_FILTER_TYPE_TAG_DATA == filter->type)  
    {  
      const TMR_TagData *fp;  
      uint8_t mask;  
  
      fp = &filter->u.tagData;  
  
      if (fp->epcByteCount > 8)  
      {  
        return TMR_ERROR_INVALID;  
      }  
  
      /* Convert the byte count to a MSB-based bit mask */  
      mask = (0xff00 >> fp->epcByteCount) & 0xff;  
  
      SETU8(msg, *i, TMR_ISO180006B_SELECT_OP_EQUALS);  
      SETU8(msg, *i, 0); /* Address - EPC is at the start of memory */  
      SETU8(msg, *i, mask);  
      for (j = 0 ; j < fp->epcByteCount; j++)  
      {  
        SETU8(msg, *i, fp->epc[j]);  
      }  
      for ( ; j < 8 ; j++)  
      {  
        SETU8(msg, *i, 0); /* EPC data must be 8 bytes */  
      }  
    }  
    else  
    {  
      return TMR_ERROR_INVALID;  
    }  
      
  }  
#endif /* TMR_ENABLE_ISO180006B */  
#endif /* TMR_ENABLE_UHF */  
  
#ifndef TMR_ENABLE_UHF  
  if (TMR_FILTER_TYPE_UID_SELECT == filter->type)  
#else  
  else if (TMR_FILTER_TYPE_UID_SELECT == filter->type)  
#endif /* TMR_ENABLE_UHF */  
  {  
#ifdef TMR_ENABLE_HF_LF  
    uint8_t len;  
    const TMR_UID_Select *fp;  
    fp = &filter->u.uidSelect;  
  
    *option |= TMR_SR_SINGULATION_OPTION_SELECT_ON_UID;  
  
    SETU8(msg, *i, fp->UIDMaskBitLen);  
    for (len = 0; len < tm_u8s_per_bits(fp->UIDMaskBitLen); len++)  
    {  
      SETU8(msg, *i, fp->UIDMask[len]);  
    }  
    SETU8(msg, *i, 0x00); //End Of Filter  
  }  
  else if (TMR_FILTER_TYPE_TAGTYPE_SELECT == filter->type)  
  {  
    TMR_Status ret;  
    const TMR_TAGTYPE_Select *fp;  
    fp = &filter->u.tagtypeSelect;  
  
    *option |= TMR_SR_SINGULATION_OPTION_SELECT_ON_TAGTYPE;  
  
    ret = TMR_SR_convertToEBV(msg, i, fp->tagType);  
    if(TMR_SUCCESS != ret)  
    {  
      return ret;  
    }  
    SETU8(msg, *i, 0x00); //End Of Filter  
  }  
  else if (TMR_FILTER_TYPE_MULTI == filter->type)  
  {  
    uint16_t filterLength, temp;  
    filterLength = filter->u.multiFilterList.len;  
  
    if(filterLength > NUMBER_OF_HF_LF_MULTISELECT_SUPPORTED)  
    {  
      return TMR_ERROR_UNSUPPORTED;  
    }  
    else  
    {  
      temp = 0;  
      filterbytes(protocol, filter->u.multiFilterList.tagFilterList[temp++], option, i, msg, accessPassword, false);  
  
      while (temp < filterLength)  
      {  
        option = &msg[(*i) - 1];  
        filterbytes(protocol, filter->u.multiFilterList.tagFilterList[temp++], option, i, msg, accessPassword, false);  
      }  
    }  
#endif /* TMR_ENABLE_HF_LF */  
  }  
  else  
  {  
    return TMR_ERROR_INVALID;  
  }  
  
  return TMR_SUCCESS;  
}

```
The filter returns an option byte by reference. This option byte tells the reader what kind of filter needs to be applied.

The option is based on the following struct:

```cpp
typedef enum TMR_SR_Gen2  
{  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_GEN2_SINGULATION_OPTION_SELECT_DISABLED         = 0x00,  
  TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_EPC           = 0x01,  
  TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_TID           = 0x02,  
  TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_USER_MEM      = 0x03,  
  TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_ADDRESSED_EPC = 0x04,  
  TMR_SR_GEN2_SINGULATION_OPTION_USE_PASSWORD            = 0x05,  
  TMR_SR_GEN2_SINGULATION_OPTION_SELECT_ON_LENGTH_OF_EPC = 0x06,  
  TMR_SR_GEN2_SINGULATION_OPTION_SELECT_GEN2TRUNCATE	 = 0x07,  
  TMR_SR_GEN2_SINGULATION_OPTION_INVERSE_SELECT_BIT      = 0x08,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA           = 0x10,  
#ifdef TMR_ENABLE_UHF  
  TMR_SR_GEN2_SINGULATION_OPTION_EXTENDED_DATA_LENGTH    = 0x20,  
#endif /* TMR_ENABLE_UHF */  
  TMR_SR_GEN2_SINGULATION_OPTION_SECURE_READ_DATA        = 0x40  
}TMR_SR_Gen2SingulationOptions;
```

Line 10 enables "secure read access", don't know what that is (question 1).
If there is no password or filter, it sets option to 0x00(SELECT_DISABLED).
The filter content is different based on the type:

```cpp
typedef enum TMR_FilterType  
{  
  /** Tag data filter - non-protocol-specific */  
  TMR_FILTER_TYPE_TAG_DATA          = 0,  
  /** Gen2 Select filter */  
  TMR_FILTER_TYPE_GEN2_SELECT       = 1,   
  /** ISO180006B Select filter */  
  TMR_FILTER_TYPE_ISO180006B_SELECT = 2,  
  /** UID based filter **/  
  TMR_FILTER_TYPE_UID_SELECT = 1,  
  /** TagType based filter **/  
  TMR_FILTER_TYPE_TAGTYPE_SELECT = 2,  
  /** Multi select filter */  
  TMR_FILTER_TYPE_MULTI = 3  
} TMR_FilterType;
```

There are a lot of if statements and different types of filters.
We made an attempt to parse the communication protocol.
There is no response to a filter, since it is not an opcode.
### **Communication**
The main structure of the filter seems to be like this.

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Access Password|4|[password]|Optional|
|Bit Pointer|4|[address]|Optional|
|Length Mask in bits|1/2|[integer]|Required|
|Filter Data|0..255|[data]|Optional|
|Target|1|[TMR_GEN2_Select_target]|Optional|
|Action|1|[TMR_GEN2_Select_action]|Optional|
|End of Select Indicator|1|00|Optional|
Some logic:
- If filter == NULL and password == NULL : no filter
- If protocol == TMR_TAG_PROTOCOL_GEN2 and password != NULL and filter == NULL : use password
- if multi-select : add target, action, and end of select indicator

There is quite a bit more to document, but we leave it like this for now.
### **Examples**
We obtained these examples from the URA.
Note that simply providing a password can also be seen as a filter.

|Type|Start|Filter Data|Target|Action|Invert|Bytecode|Code|
|---|---|---|---|---|---|---|---|
|EPC ID|0x20|0x9812AB|Select|0|no|00 00 00 00<br><br>00 00 00 20<br><br>18<br><br>98 12 AB<br><br>04<br><br>00<br><br>00|14 (1C with invert)|
|User|0x00|0x11223344|Select|1|no|00 00 00 00<br><br>00 00 00 00<br><br>20<br><br>11 22 33 44<br><br>04<br><br>01<br><br>00|13 (1B with invert)|
|EPC length|NA|0x70|Select|0|no|00 70<br><br>04<br><br>00<br><br>00|16 (16 with invert)|
|TID|0x10|0x8812AB|Select|3|no|00 00 00 00<br><br>00 00 00 10<br><br>18<br><br>88 12 AB<br><br>04<br><br>03<br><br>00|12 (1A with invert)|
|Password||0x12345678||||11 22 33 44<br><br>04<br><br>00<br><br>00|5|

The password is kind of strange but we follow the procedures from the source code.
I cannot find any real message where this filter is applied.
If mutiselect is disabled, the last 3 bytes will not be there.
If length is larger then 255, extended data length (+ 0x20) is set
### **Implementation**
Implementation of the filter is probably the most complex because of the many possibilities.
We use the examples defined above as test-cases to see if our code returns the right output.
We created a function constructFilterMsg that takes care of constructing a filter message.
## **READ_TAG_ID_MULTIPLE**
This opcode (0x22) reads multiple tags to buffer.
This is the default way of finding tags in the universal reader assistant.
### **Code**

For sending requests, we have the following code.
```c++ fold title:"Read Multiple Tags"
TMR_Status  
TMR_SR_msgSetupReadTagMultipleWithMetadata(TMR_Reader *reader, uint8_t *msg, uint8_t *i, uint16_t timeout,  
                               TMR_SR_SearchFlag searchFlag,  
							                 TMR_TRD_MetadataFlag metadataFlag,  
                               const TMR_TagFilter *filter,  
                               TMR_TagProtocol protocol,  
                               TMR_GEN2_Password accessPassword)  
{  
  TMR_Status ret;  
  TMR_SR_SerialReader *sr;  
  uint8_t optbyte;  
#ifdef TMR_ENABLE_UHF  
  uint8_t SingulationOption = 0;  
  uint8_t planCount = 0;  
#endif /* TMR_ENABLE_UHF */  
  sr = &reader->u.serialReader;  
  sr->opCode = TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE;  
  ret = TMR_SUCCESS;  
  
#ifdef TMR_ENABLE_UHF  
  isMultiSelectEnabled = ((filter) && (filter->type == TMR_FILTER_TYPE_TAG_DATA)) ? false : true;  
#endif /* TMR_ENABLE_UHF */  
  
  SETU8(msg, *i, TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE);  
  
#ifdef TMR_ENABLE_UHF  
  /**  
   * we need to add the option for bap parameters if enabled  
   *   
   * for adding the bap parameter option, and EBV technique is used  
   * raising the lowest order bit of the high opder byte signals the  
   * use of new Gen2 Bap support parameters.  
   **/  
  if (sr->isBapEnabled)  
  {  
    SingulationOption = TMR_SR_TAGOP_BAP;  
  }  
  
  /**  
    * In Embedded ReadAfterWrite operation, option field should be preceded by 0x84.  
    * Adding the field in msg.  
    **/  
  if (reader->isReadAfterWrite)  
  {  
    SingulationOption |= TMR_SR_TAGOP_READ_AFTER_WRITE;  
  }  
  
  /* To perform Multiple select operation, option byte should be preceded by 0x88.*/  
  if(isMultiSelectEnabled)  
  {  
    SingulationOption |= TMR_SR_TAGOP_MULTI_SELECT;  
    /* Enable isEmbeddedTagopEnabled flag to indicate that 0x88 option has already been added. */  
    if((reader->readParams.readPlan->type == TMR_READ_PLAN_TYPE_SIMPLE) && (reader->readParams.readPlan->u.simple.tagop != NULL))  
    {  
      isEmbeddedTagopEnabled = true;  
    }  
#ifndef TMR_ENABLE_GEN2_ONLY  
    else if(reader->readParams.readPlan->type == TMR_READ_PLAN_TYPE_MULTI)  
    {  
      for(planCount = 0; planCount < reader->readParams.readPlan->u.multi.planCount; planCount++)  
      {  
        if(reader->readParams.readPlan->u.multi.plans[planCount]->u.simple.tagop != NULL)  
        {  
          isEmbeddedTagopEnabled = true;  
        }  
      }  
    }  
#endif /* TMR_ENABLE_GEN2_ONLY */  
  }  
  
  if(SingulationOption)  
  {  
    SETU8(msg, *i, SingulationOption);  
  }  
#endif /* TMR_ENABLE_UHF */  
  optbyte = *i;  
  SETU8(msg, *i, 0); /* Initialize option byte */  
  
  /* add the large tag population support */  
  searchFlag = (TMR_SR_SearchFlag)(searchFlag   
        | TMR_SR_SEARCH_FLAG_LARGE_TAG_POPULATION_SUPPORT);  
  
  if (reader->continuousReading)  
  {  
    searchFlag = (TMR_SR_SearchFlag)(searchFlag  
        | TMR_SR_SEARCH_FLAG_TAG_STREAMING);  
  
    /**  
     * We need to send a different flag for the new reader stats,  
     * Both these options are mutually exclusive in nature,  
     * Hence can not be raised at once.  
     **/  
#ifdef TMR_ENABLE_UHF  
    if (TMR_SR_STATUS_NONE != reader->streamStats)  
    {  
      searchFlag |= TMR_SR_SEARCH_FLAG_STATUS_REPORT_STREAMING;  
    }  
    else  
#endif /* TMR_ENABLE_UHF */  
    {  
      if (TMR_READER_STATS_FLAG_NONE != reader->statsFlag)  
      {  
        searchFlag |= TMR_SR_SEARCH_FLAG_STATS_REPORT_STREAMING;  
      }  
    }  
  }  
  
  /**  
   * Add the fast search flag depending on the user choice  
   */  
#ifdef TMR_ENABLE_UHF  
  if (reader->fastSearch)  
  {  
    searchFlag = (TMR_SR_SearchFlag)(searchFlag  
      |TMR_SR_SEARCH_FLAG_READ_MULTIPLE_FAST_SEARCH);  
    reader->fastSearch = false;  
  }  
#endif /* TMR_ENABLE_UHF */  
  /**  
   * Add the trigger read flag depending on the user choice  
   */  
  if (reader->triggerRead)  
  {  
    searchFlag = (TMR_SR_SearchFlag)(searchFlag  
      |TMR_SR_SEARCH_FLAG_GPI_TRIGGER_READ);  
    reader->triggerRead = false;  
  }  
  
  /**  
   * Add the duty cycle flag for async read  
   */  
  if (reader->continuousReading)  
  {  
    searchFlag = (TMR_SR_SearchFlag)(searchFlag  
        |TMR_SR_SEARCH_FLAG_DUTY_CYCLE_CONTROL);  
  }  
  
  if (reader->isStopNTags)  
  {  
    searchFlag = (TMR_SR_SearchFlag)(searchFlag  
        |TMR_SR_SEARCH_FLAG_RETURN_ON_N_TAGS);  
  }  
  
  SETU16(msg, *i, searchFlag);  
  SETU16(msg, *i, timeout);  
  
  /* Frame 2 bytes of async off time in case of continuous read. */  
  if (reader->continuousReading)  
  {  
#ifndef TMR_ENABLE_GEN2_ONLY  
    if(TMR_READ_PLAN_TYPE_MULTI == reader->readParams.readPlan->type)  
    {  
      SETU16(msg, *i, (uint16_t)reader->subOffTime);  
    }  
    else  
#endif /* TMR_ENABLE_GEN2_ONLY */  
    {  
	  uint32_t offtime;  
	  ret = TMR_paramGet(reader,TMR_PARAM_READ_ASYNCOFFTIME,&offtime);  
	  SETU16(msg, *i, (uint16_t)offtime);  
    }  
  }  
  SETU16(msg, *i, metadataFlag);  
  
  /**  
   * Add the no of tags to be read requested by user  
   * in stop N tag reads.  
   **/  
  if (reader->isStopNTags)  
  {  
    SETU32(msg , *i, (uint32_t)reader->numberOfTagsToRead);  
  }  
  
  if (reader->continuousReading)  
  {  
    if (TMR_READER_STATS_FLAG_NONE != reader->statsFlag)  
    {  
      /**  
       * To extend the flag byte, an EBV technique is to be used.  
       * When the highest order bit of the flag byte is used,  
       * it signals the reader’s parser, that another flag byte is to follow  
       */  
        SETU16(msg, *i, (uint16_t)reader->statsFlag);  
    }  
#ifdef TMR_ENABLE_UHF  
    else  
    {  
      if (TMR_SR_STATUS_NONE != reader->streamStats)  
      {  
        /* Add status report flags, so that the status stream responses are received */  
        SETU16(msg, *i, (uint16_t)reader->streamStats);  
      }  
    }  
#endif /* TMR_ENABLE_UHF */  
  }  
  
#ifdef TMR_ENABLE_ISO180006B  
  /*  
   * Earlier, this filter bytes were skipped for a null filter and gen2 0 access password.  
   * as the filterbytes it self has the checks internally, these were removed.  
   * for some protocols (such as ISO180006B) the "null" filter is not zero-length, but we don't need to send  
   * that with this command.  
   */  
  if (TMR_TAG_PROTOCOL_ISO180006B == protocol && NULL == filter)  
  {  
    /* ISO18000-6B should not include any filter arg bytes if null */  
  }  
  else  
#endif /* TMR_ENABLE_ISO180006B */  
  {  
    ret = filterbytes(protocol, filter, &msg[optbyte], i, msg,  
        accessPassword, true);  
  }  
  
  msg[optbyte] |= TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA;  
  
#ifdef TMR_ENABLE_UHF  
  if (isSecureAccessEnabled)  
  {  
    msg[optbyte] |= TMR_SR_GEN2_SINGULATION_OPTION_SECURE_READ_DATA;  
  }  
#endif /* TMR_ENABLE_UHF */  
  return ret;  
}  
```

The response to this request is actually already given in the library code.
This response is based on the official documentation.
### **Communication**
We found the following template for sending requests:

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Header|1|FF|Required|
|Length|1|[length of message]|Required|
|Opcode|1|0x22 (TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE)|Required|
|Tag Operation Mode|1|1. 0x81 (TMR_SR_TAGOP_BAP)<br><br>2. 0x84 (TMR_SR_TAGOP_READ_AFTER_WRITE)<br><br>3. 0x88 (TMR_SR_TAGOP_MULTI_SELECT)|Optional|
|Option Byte|1|[TMR_SR_Gen2SingulationOptions]|Required|
|Search Flag|2|[TMR_SR_SearchFlag] (default 00 13)|Required|
|Timeout|2|[user input usually 03 E8]|Required|
|Duty Cycle Offtime|2|[integer]|Optional|
|Metadata Flag|2|[TMR_TRD_MetadataFlag] (default 00 57)|Optional (required for m6e)|
|Stream Stats|2|[TMR_Reader_StatsFlag]|Optional (required for continuous reading)|
|Number of Tags to Read|4|[number of tags to read]|Optional (required for read n tags)|
|Filter|...||Optional|
|checksum|2|[checksum of message]|Required|

Below, we show a few examples.

From the URA we have the following messages, without and with filter:

```
FF 08 22 // header, length, opcode   
88 // tag operation mode --> select multiple tags    
10 // option byte --> metadata   
00 13 // search flag  
03 E8 // timeout  
00 57 // metadata flag    
1F DF // checksum

FF 17 22 // header, length, opcode    
88    
14    
00 13    
03 E8    
00 57    
00 00 00 00 // password  
00 00 00 20 // start address  
18 // filter data length in bits  
98 12 AB // filter data  
04 00 00 // target, action, end of select  
39 B2 // checksum
```

### **Implementation**
We made a function called constructReadTagIdMultipleMsg that creates a 0x22 message based on its input parameters.
## WRITE_TAG_ID
This opcode is only supported on Gen2 tags.
This should not be a problem since this is the main type anyways.
### Code
```cpp fold title:"Write Tag EPC"
TMR_Status  
TMR_SR_cmdWriteGen2TagEpc(TMR_Reader *reader, const TMR_TagFilter *filter, TMR_GEN2_Password accessPassword,   
					  uint16_t timeout, uint8_t count, const uint8_t *id, bool lock, TMR_uint8List* response)  
{  
  TMR_Status ret;  
  uint8_t msg[TMR_SR_MAX_PACKET_SIZE];  
  uint8_t i, optbyte;  
  
  i = 2;  
  TMR_SR_cmdFrameHeader(msg, &i, TMR_SR_OPCODE_WRITE_TAG_ID, timeout, false);  
  
  optbyte = i;  
  SETU8(msg, i, 0);  
    
  
  ret = filterbytes(TMR_TAG_PROTOCOL_GEN2, filter, &msg[optbyte], &i, msg,  
                    accessPassword, true);  
  if (TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
  
  if (0 == msg[optbyte])  
  {  
	  SETU8(msg, i, 0);  // Initialize second RFU byte to zero  
  }  
  
  if (i + count + 1 > TMR_SR_MAX_PACKET_SIZE)  
  {  
    return TMR_ERROR_TOO_BIG;  
  }  
  
  memcpy(&msg[i], id, count);  
  i += count;  
  msg[1] = i - 3; /* Install length */  
  
  ret = TMR_SR_sendTimeout(reader, msg, timeout);  
  if (TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
  
  if (NULL != response)  
  {  
    i = TMR_WRDATA_RESP_IDX;  
  
    /* Write Tag Data doesn't put actual tag data inside the metadata fields.  
     * Read the actual data here (remainder of response.) */  
    {  
      response->len = msg[1] + (TMR_DEF_RESPONSE_CMD_LEN - i);  
      (response->len > response->max) ? (response->len = response->max) : (response->len = response->len);  
      memcpy(response->list, &msg[i], response->len);  
      i += response->len;  
     }  
  }  
  
  return TMR_SUCCESS;  
}
```
The code above writes an EPC to tag.
### Communication

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Header|1|0xFF|Required|
|Length|1|[Integer]|Required|
|Opcode|1|0x23 (TMR_SR_OPCODE_WRITE_TAG_ID)|Required|
|Timeout|2|[Integer]|Required|
|Multiselect Opcode|1|0x88 (TMR_SR_TAGOP_MULTI_SELECT)|Optional (only if multiple writes)|
|Option Byte|1|[TMR_SR_Gen2SingulationOptions]|Required|
|Filter|...||Optional|
|Message|...||Required|
|Checksum|2||Required|
The following requests are writing a new EPC:
```
FF // header  
20 // length    
23 // opcode TMR_SR_OPCODE_WRITE_TAG_ID (serial_reader_imp.h)  
03  E8 // timeout  
01 // option initialize (filter?)   
00  00  00 00 // address  
60  // bits?  
98 12  AB  32  FF 00  02  06  05  E4  01  F4  // old epc  
98 12  AB  32  FF  00  02  06  05 E4  01  F5  // new epc   
5D  8F // checksum

FF // header  
12 // length  
23 // opcode   
03  E8 // timeout  
01 // filter   
00  00  00  00 // address    
20 // size in bits  
AA  BB  CC  DD // old EPC   
AA  BB  CC  DD  EE  FF  // new EPC  
2F  FA // checksum
```

## WRITE_TAG_DATA
Write tag data works for any bank and any supported protocol.
### Code
```cpp fold title:"Write Tag Data"
TMR_Status  
TMR_SR_cmdWriteMemory(TMR_Reader *reader, TMR_ExtTagOp *tagOp, const TMR_TagFilter *filter,   
                        TMR_uint8List tagOpExtParams, TMR_uint8List* data)  
{  
  TMR_Status ret;  
  uint16_t timeout;  
  uint16_t dataLength;  
  uint8_t msg[TMR_SR_MAX_PACKET_SIZE];  
  uint8_t i = 2, optbyte = 0;  
  TMR_SR_SerialReader *sr = &reader->u.serialReader;  
  TMR_Memory_Type writeMemType = tagOp->writeMem.memType;  
  
  timeout = (uint16_t)sr->commandTimeout;  
  dataLength = tagOp->writeMem.data.len;  
  
  if (TMR_TAGOP_EXT_TAG_MEMORY == tagOp->writeMem.memType)  
  {  
    writeMemType = TMR_TAGOP_TAG_MEMORY;  
  }  
  
  TMR_SR_msgAddWriteMemory(msg, &i, timeout, writeMemType, tagOp->writeMem.address);  
  optbyte = 5;  
  
  //Assemble access password.  
  TMR_SR_msgAddAccessPassword(msg,  &i, &optbyte, tagOp->accessPassword);  
  
#if TMR_ENABLE_EXTENDED_TAGOPS  
  //Assemble extended params.  
  ret = TMR_SR_msgAddExtendedParams(msg, &i, &optbyte, &tagOpExtParams);  
  if (TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
#endif /* TMR_ENABLE_EXTENDED_TAGOPS */  
  
  //Assemble filter data.  
  ret = filterbytes(reader->u.serialReader.currentProtocol, filter, &msg[optbyte], &i, msg, 0, false);  
  if(TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
  
  //Return error if the serial command has already reached the limit.  
  if (i + dataLength + 1 > TMR_SR_MAX_PACKET_SIZE)  
  {  
    return TMR_ERROR_TOO_BIG;  
  }  
  
  //Assemble data to be written.  
  memcpy(&msg[i], tagOp->writeMem.data.list, dataLength);  
  i += dataLength;  
  
  ret = TMR_SR_sendCmd(reader, msg, i);  
  if (TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
  
  if (NULL != data)  
  {  
    i = 5;  
  
    /* Write Tag Data doesn't put actual tag data inside the metadata fields.  
     * Read the actual data here (remainder of response.) */  
    {  
      uint16_t dataLength;  
  
      dataLength = msg[1] + 5 - i;  
      if (dataLength > data->max)  
      {  
        return TMR_ERROR_OUT_OF_MEMORY;  
      }  
      data->len = dataLength;  
  
      memcpy(data->list, &msg[i], dataLength);  
      i += dataLength;  
    }  
  }  
  
  return TMR_SUCCESS;  
}
```

### Communication

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Header|1|0xFF|Required|
|Length|1|[Integer]|Required|
|Opcode|1|0x24 (TMR_SR_OPCODE_WRITE_TAG_DATA)|Required|
|Timeout|2|[Integer]|Required|
|Multiselect Opcode|1|0x88 (TMR_SR_TAGOP_MULTI_SELECT)|Optional (only if multiple writes)|
|Option Byte|1|[TMR_SR_Gen2SingulationOptions]|Required|
|Address|4|[address]|Required|
|Bank|1|[0..3]|Required|
|Filter|...||Optional|
|Message|...||Required|
|Checksum|2||Required|

## READ_TAG_DATA
Read tag data can read data from any tag it supports.
### Code
```cpp fold title:"Read Tag Data"
MR_Status  
TMR_SR_cmdReadMemory(TMR_Reader *reader, TMR_ExtTagOp *tagOp, const TMR_TagFilter *filter,   
                       TMR_uint8List *data, TMR_uint8List tagOpExtParams)  
{  
  TMR_Status ret;  
  uint16_t timeout;  
  uint8_t msg[TMR_SR_MAX_PACKET_SIZE];  
  uint8_t i = 2, optbyte = 0;  
  TMR_SR_SerialReader *sr = &reader->u.serialReader;  
  bool withMetaData = true;  
  TMR_Memory_Type readMemType = tagOp->readMem.memType;  
  
  timeout = (uint16_t)sr->commandTimeout;  
  
  if (TMR_TAGOP_EXT_TAG_MEMORY == tagOp->readMem.memType)  
  {  
    withMetaData = false;  
    readMemType = TMR_TAGOP_TAG_MEMORY;  
  }  
  
  TMR_SR_msgAddReadMemory(msg, &i, timeout, readMemType, tagOp->readMem.address,  
                            tagOp->readMem.len, withMetaData);  
  optbyte = 5;  
  
  //Assemble access password.  
  TMR_SR_msgAddAccessPassword(msg,  &i, &optbyte, tagOp->accessPassword);  
  
#if TMR_ENABLE_EXTENDED_TAGOPS  
  //Assemble extended params.  
  ret = TMR_SR_msgAddExtendedParams(msg, &i, &optbyte, &tagOpExtParams);  
  if (TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
#endif /* TMR_ENABLE_EXTENDED_TAGOPS */  
  
  //Assemble filter data.  
  ret = filterbytes(reader->u.serialReader.currentProtocol, filter, &msg[optbyte], &i, msg, 0, false);  
  if(TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
  
  if (withMetaData)  
  {  
    msg[optbyte] |= TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA;  
    optbyte++;  
    SETU16(msg, optbyte, 0);  
  }  
  
  ret = TMR_SR_sendCmd(reader, msg, i);  
  if (TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
  if (NULL != data)  
  {  
    i = 5;  
    optbyte = msg[i];  
  
    //Send data from option byte if extended tag option is enabled.  
    if (!(optbyte & TMR_SR_SINGULATION_OPTION_EXT_TAGOP_PARAMS))  
    {  
      //skip option byte.  
      i++;  
  
      //skip metadata flags.  
      if (optbyte & TMR_SR_GEN2_SINGULATION_OPTION_FLAG_METADATA)  
      {  
        i += 2;  
      }  
    }  
  
    /* Read Tag Data doesn't put actual tag data inside the metadata fields.  
     * Read the actual data here (remainder of response.) */  
    {  
      uint16_t dataLength;  
      
      dataLength = msg[1] + 5 - i;  
      if (dataLength > data->max)  
      {  
        return TMR_ERROR_OUT_OF_MEMORY;  
      }  
      data->len = dataLength;  
  
      memcpy(data->list, &msg[i], dataLength);  
      i += dataLength;  
    }  
  }  
  
  return TMR_SUCCESS;  
}

```
Message is constructed by calling TMR_SR_msgAddReadMemory, followed by TMR_SR_msgAddAccessPassword.

Potentially, TMR_SR_msgAddExtendedParams is also called.

Lastly, it adds a filter.

It changes the option byte a bit after that.

The TMR_SR_msgAddReadMemory function is the most important:

```cpp 
void  
TMR_SR_msgAddReadMemory(uint8_t *msg, uint8_t *i, uint16_t timeout, TMR_Memory_Type memType, uint32_t address,  
                        uint8_t len, bool withMetaData)  
{  
  SETU8(msg, *i, TMR_SR_OPCODE_READ_TAG_DATA); //0x28 command  
  SETU16(msg, *i, timeout);  
  SETU8(msg, *i, 0x00); //option  
  if (withMetaData)  
  {  
    SETU16(msg, *i, 0); // metadata flags - initialize  
  }  
  SETU8(msg, *i, memType);  
  if(isAddrByteExtended)  
  {  
    SETU32(msg, *i, address);  
  }  
  else  
  {  
    SETU8(msg, *i, address);  
  }  
  SETU8(msg, *i, len);  
}
```

It shows the opcode, followed by the timeout, option byte, optional metadata, memory bank, address, and length.

We don't implement the TMR_SR_msgAddExtendedParams function.

### Communication

Communication is simpler than the previous opcode.

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Header|1|0xFF|Required|
|Length|1|[length of message]|Required|
|Opcode|1|0x28 (TMR_SR_OPCODE_READ_TAG_DATA)|Required|
|Timeout|2|[Integer]|Required|
|Option Byte|1|[TMR_SR_Gen2SingulationOptions]|Required|
|Metadata|2|[TMR_TRD_MetadataFlag]|Optional (only if useMetadata is enabled)|
|Bank|1|[0..3]|Required|
|Address|4|[address to start reading]|Required|
|Length|1|[2 bytes to read]|Required|
|Filter|...||Optional|
|Checksum|2|[checksum of message]|Required|

|Command|Bytecode|
|---|---|
|Tag filter 9812AB32FF00020605E401F6 with timeout 03 E8|FF 1C 28 // header, length, and opcode<br><br>03 E8 // timeout<br><br>11 // option byte<br><br>00 00 // metadata<br><br>00 // bank<br><br>00 00 00 00 // address<br><br>00 // length<br><br>00 00 00 00 // access password<br><br>60 // filter lenght in bits<br><br>98 12 AB 32 FF 00 02 06 05 E4 01 F6 // filter<br><br>BA 85 // checksum|
|No filter (from library, hope it is correct)|FF 1C 28<br><br>03 E8<br><br>10<br><br>00 00<br><br>00<br><br>00 00 00 00<br><br>00<br><br>BA 85|
|||
|||
|||

### Implementation

Like with the previous function, we created a function called constructReadTagDataMsg that constructs a _READ_TAG_DATA_ message.

## GET_TAG_ID_BUFFER

Gets the data from the buffer.

### Code

```cpp
TMR_Status TMR_SR_cmdGetTagsRemaining(TMR_Reader *reader, uint16_t *tagCount)  
{  
  uint8_t msg[TMR_SR_MAX_PACKET_SIZE];  
  TMR_Status ret;  
  ret = TMR_SR_callSendCmd(reader, msg, TMR_SR_OPCODE_GET_TAG_ID_BUFFER);  
  if (TMR_SUCCESS != ret)  
  {  
    return ret;  
  }  
  *tagCount = GETU16AT(msg, 7) - GETU16AT(msg, 5);  
  return TMR_SUCCESS;  
}

Does not really do anything. The main function is in TMR_SR_callSendCmd.

That function just sends the opcode.

It is actually specified in serial_reader.c:1877:

SETU8(msg, i, TMR_SR_OPCODE_GET_TAG_ID_BUFFER);  
SETU16(msg, i, reader->userMetadataFlag);  
SETU8(msg, i, 0); /* read options */
```

### Communication

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Header|1|0xFF|Required|
|Length|1|[length of message]|Required|
|Opcode|1|0x29|Required|
|Metadata|2|[TMR_TRD_MetadataFlag] (0x00 0x57)|Required|
|Options Byte|1|0x00|Required|
|Checksum|2|[checksum of message]|Required|

A simple example is FF 03 29 00 57 00 A3 22.

As can be seen, it follows the protocol.

## MULTI_PROTOCOL_TAG_OP

_MULTI_PROTOCOL_TAG_OP_ is more like a helper function.

In our case we are using it for continuous reading.

### Code

```cpp fold title:"Multiprotocol Read"
/* Helper function to frame the multiple protocol search command */  
TMR_Status   
TMR_SR_msgSetupMultipleProtocolSearch(TMR_Reader *reader, uint8_t *msg, TMR_SR_OpCode op, TMR_TagProtocolList *protocols, TMR_TRD_MetadataFlag metadataFlags, TMR_SR_SearchFlag antennas, TMR_TagFilter **filter, uint16_t timeout)  
{  
  TMR_Status ret;  
  uint8_t i = 0, j = 0;  
  uint16_t subTimeout = 0;  
  uint32_t asyncOffTime = 0;  
  uint16_t searchFlags = 0;  
  
  ret = TMR_SUCCESS;  
  i=2;  
  
#if defined(TMR_ENABLE_BACKGROUND_READS)|| defined(SINGLE_THREAD_ASYNC_READ)  
  asyncOffTime = reader->readParams.asyncOffTime;  
#endif  
  
  SETU8(msg, i, TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP);  //Opcode  
  if(reader->continuousReading)  
  {  
    /* Timeout should be zero for true continuous reading */  
    SETU16(msg, i, 0);  
    SETU8(msg, i, (uint8_t)0x1);//TM Option 1, for continuous reading  
  }  
  else  
  {  
    SETU16(msg, i, timeout); //command timeout  
    SETU8(msg, i, (uint8_t)0x11);//TM Option, turns on metadata  
    SETU16(msg, i, (uint16_t)metadataFlags);  
  }  
  
  SETU8(msg, i, (uint8_t)op);//sub command opcode  
  
#ifndef TMR_ENABLE_GEN2_ONLY  
  if (TMR_READ_PLAN_TYPE_MULTI == reader->readParams.readPlan->type)  
  {  
    reader->isStopNTags = false;  
    /**  
    * in case of multi read plan look for the stop N trigger option  
    **/  
    for (j = 0; j < reader->readParams.readPlan->u.multi.planCount; j++)  
    {  
      if (reader->readParams.readPlan->u.multi.plans[j]->u.simple.stopOnCount.stopNTriggerStatus)  
      {  
        reader->numberOfTagsToRead += reader->readParams.readPlan->u.multi.plans[j]->u.simple.stopOnCount.noOfTags;  
        reader->isStopNTags = true;  
      }  
    }  
  }  
  else  
#endif /* TMR_ENABLE_GEN2_ONLY */  
  {  
    reader->numberOfTagsToRead = reader->readParams.readPlan->u.simple.stopOnCount.noOfTags;  
  }  
  
#ifdef TMR_ENABLE_HF_LF  
  if (reader->isProtocolDynamicSwitching)  
  {  
    searchFlags |= (uint16_t)(TMR_SR_SEARCH_FLAG_DYNAMIC_PROTOCOL_SWITCHING);  
  }  
#endif /* TMR_ENABLE_HF_LF */  
  
  if (reader->isStopNTags)  
  {  
    /**  
    * True means atleast one sub read plan has the requested for stop N trigger.  
    */  
    searchFlags |= (uint16_t)(TMR_SR_SEARCH_FLAG_RETURN_ON_N_TAGS);  
  }  
  
  //Add search flags.  
  SETU16(msg, i, (uint16_t)searchFlags);  
  
  if (reader->isStopNTags)  
  {  
    //Add the total tag count for stop N trigger.  
    SETU32(msg, i, reader->numberOfTagsToRead);  
  }  
  
  /**  
  * TODO:add the timeout as requested by the user  
  **/  
  subTimeout =(uint16_t)(timeout/(protocols->len));  
  
  for (j=0;j<protocols->len;j++) // iterate through the protocol search list  
  {  
    int PLenIdx;  
  
    TMR_TagProtocol subProtocol=protocols->list[j];  
    SETU8(msg, i, (uint8_t)(subProtocol)); //protocol ID  
    PLenIdx = i;  
    SETU8(msg, i, 0); //PLEN  
  
    /**  
    * in case of multi readplan and the total weight is not zero,  
    * we should use the weight as provided by the user.  
    **/  
#ifndef TMR_ENABLE_GEN2_ONLY  
    if (TMR_READ_PLAN_TYPE_MULTI == reader->readParams.readPlan->type)  
    {  
      if (0 != reader->readParams.readPlan->u.multi.totalWeight)  
      {  
        subTimeout = (uint16_t)(reader->readParams.readPlan->u.multi.plans[j]->weight * timeout  
          / reader->readParams.readPlan->u.multi.totalWeight);   
        reader->subOffTime =  (uint16_t)(reader->readParams.readPlan->u.multi.plans[j]->weight * asyncOffTime  
          / reader->readParams.readPlan->u.multi.totalWeight);   
      }  
      else  
      {  
        reader->subOffTime =(uint16_t)(asyncOffTime /(protocols->len));  
      }  
  
      /**  
      * In case of Multireadplan, check each simple read plan FastSearch option and  
      * stop N trigger option.  
      */  
      reader->triggerRead = reader->readParams.readPlan->u.multi.plans[j]->u.simple.triggerRead.enable;  
#ifdef TMR_ENABLE_UHF  
      reader->fastSearch = reader->readParams.readPlan->u.multi.plans[j]->u.simple.useFastSearch;  
#endif /* TMR_ENABLE_UHF */  
      reader->isStopNTags = reader->readParams.readPlan->u.multi.plans[j]->u.simple.stopOnCount.stopNTriggerStatus;  
      reader->numberOfTagsToRead = reader->readParams.readPlan->u.multi.plans[j]->u.simple.stopOnCount.noOfTags;  
    }  
#endif /* TMR_ENABLE_GEN2_ONLY */  
  
    switch(op)  
    {  
    case TMR_SR_OPCODE_READ_TAG_ID_MULTIPLE:  
      {  
        TMR_ReadPlan *plan;  
        /**  
        * simple read plan uses this function, only when tagop is NULL,  
        * s0, need to check for simple read plan tagop.  
        **/  
#ifndef TMR_ENABLE_GEN2_ONLY  
        if (TMR_READ_PLAN_TYPE_MULTI == reader->readParams.readPlan->type)  
        {  
          plan = reader->readParams.readPlan->u.multi.plans[j];  
        }  
        else  
#endif /* TMR_ENABLE_GEN2_ONLY */  
        {  
          plan = reader->readParams.readPlan;  
        }  
        {  
          /* check for the tagop */  
          if (NULL != plan->u.simple.tagop)  
          {  
#ifdef TMR_ENABLE_UHF  
            uint32_t readTimeMs = (uint32_t)subTimeout;  
            uint8_t lenbyte = 0;  
            //add the tagoperation here  
            ret = TMR_SR_addTagOp(reader,plan->u.simple.tagop, plan, msg, &i, readTimeMs, &lenbyte);  
  
            /* Install length of subcommand */  
            msg[lenbyte] = i - (lenbyte + 2);  
#endif /* TMR_ENABLE_HF_LF */  
          }  
          else  
          {  
            ret = TMR_SR_msgSetupReadTagMultipleWithMetadata(reader, msg, &i, subTimeout, antennas, metadataFlags ,filter[j], subProtocol, 0);  
          }  
        }  
        break;  
      }  
    default :  
      {  
        return TMR_ERROR_INVALID_OPCODE;            
      }  
    }  
  
    msg[PLenIdx]= i - PLenIdx - 2; //PLEN  
    msg[1]=i - 3;  
  }  
  return ret;  
}
```
First, it sets some variables. It starts writing the opcode _TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP_.

We assume continuous reading is enabled. In that case timeout is set to 0 and we write 0x01 to indicate continuous reading.

If not continuous, the timeout is user specified and it also includes metadata.

After that it determines the number of tags to read, followed by setting up and initializing the searchFlags.

After that the sub-command is added for every protocol specified in the list.

In our case we assume there is only GEN2.

For every protocol it stores the subprotocol and PLEN value.

The plen is the length of the subcommand I think.

### Communication

Communication is relatively complex because it has a subcommand and possibly a filter.

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Header|1|FF|Required|
|Length|1|[length of message]|Required|
|Opcode|1|0x2F (TMR_SR_OPCODE_MULTI_PROTOCOL_TAG_OP)|Required|
|Timeout|2|[integer] (0 if continuous)|Required|
|Option Byte|1|[0x01(continuous) or 0x11]|Required|
|Metadata Flag|2|[TMR_TRD_MetadataFlag]|Optional (not continuous)|
|Subcommand Opcode|1|0x22 (in our case)|Required|
|Search Flags|2|[TMR_SR_SearchFlag]|Required|
|Number of tags|4|[integer]|Optional (stop after n tags)|
|Subprotocol|1|[GEN2...]|Required|
|Subcommand Length|1|[length of subcommand]|Required|
|Subcommmand|...|[0x22 command]|Optional|
|Checksum|2|[checksum of message]|Required|

If we take the example from the Arduino Library:

```
00 00 // timeout   
01 // option byte  
22 // subcommand opcode  
00 00 // search flags  
05 // subprotocol  
07 // preamble length  
22 // opcode  
10 // option byte  
00 1B // search flag  
03 E8 // timeout  
01 FF // checksum
```

URA sends this one without a filter:

```
FF 15 2F // header, length, opcode   
00 00 // timeout  
01 // option byte  
22 // subcommand opcode  
00 00 // search flags  
05 // protocol  
0C // subcommand length  
22 // subcommand opcode --> subcommand starts here  
88 // tag operation mode  
10 // option byte  
05 1B // search flag  
03 E8 // timeout  
00 C8 // offtime   
00 57 // metadata  
01 00 // stream stats  
C9 DA // checksum
```

We have another one with a read filter for EPC:

```
FF 23 2F // header, length, opcode  
00 00 // timeout    
01 // option byte   
22 // subcommand opcode   
00 00 // search flags   
05 // protocol   
1A // subcommand length    
22 // subcommand opcode   
88 // tag operation mode  
14 // option byte --> EPC filter and metadata  
05 1B // search flag  
03 E8 // timeout  
00 FA // offtime = 250  
00 57 // metadata   
01 00 // stream stats   
00 00 00 00 // password  
00 00 00 20 // start address 32  
10 // length of data in bits   
E2 00 // filter  
04 00  00 // target, action, end of select  
46  C5 // checksum
```

### Implementation

We don't need to think about anything else than Gen2.

However, we should include the "_Duty Cycle Offtime_" as a parameter because it can be used to optimize performance.

Also, the subcommand should be able to take a filter.

We make two functions, one with default settings that should give the URA example without filter.

The second one takes a readConfig and filter as input and should be flexible enough to build anything.

# **Parsing Responses**

The way responses are parsed in the Arduino Library are incorrect.

It does not take into account that the metadata included can be different.

The function responsible for parsing metadata is called TMR_SR_parseMetadataOnly:

```cpp fold title:"Parse Metadata"
TMR_Status  
TMR_SR_parseMetadataOnly(TMR_Reader *reader, TMR_TagReadData *read, uint16_t flags,  
                                uint8_t *i, uint8_t msg[])  
{  
  TMR_Status ret = TMR_SUCCESS;  
  
  read->metadataFlags = flags;  
#ifdef TMR_ENABLE_UHF  
  read->gpioCount = 0;  
#endif /* TMR_ENABLE_UHF */  
  read->antenna = 0;  
  read->dspMicros = 0;  
  
  reader->userMetadataFlag = read->metadataFlags;  
  
  /* Fill in tag data from response */  
  if (flags & TMR_TRD_METADATA_FLAG_READCOUNT)  
  {  
    read->readCount = GETU8(msg, *i);  
  }  
#ifdef TMR_ENABLE_UHF  
  if (flags & TMR_TRD_METADATA_FLAG_RSSI)  
  {  
    read->rssi = (int8_t)GETU8(msg, *i);  
  }  
#endif /* TMR_ENABLE_UHF */  
  if (flags & TMR_TRD_METADATA_FLAG_ANTENNAID)  
  {  
    read->antenna = GETU8(msg, *i);  
  }  
#ifdef TMR_ENABLE_UHF  
  if (flags & TMR_TRD_METADATA_FLAG_FREQUENCY)  
  {  
    read->frequency = GETU24(msg, *i);  
  }  
#endif /* TMR_ENABLE_UHF */  
  if (flags & TMR_TRD_METADATA_FLAG_TIMESTAMP)  
  {  
    read->dspMicros = GETU32(msg, *i);  
  }  
#ifdef TMR_ENABLE_UHF  
  if (flags & TMR_TRD_METADATA_FLAG_PHASE)  
  {  
    read->phase = GETU16(msg, *i);  
  }  
#endif /* TMR_ENABLE_UHF */  
  if (flags & TMR_TRD_METADATA_FLAG_PROTOCOL)  
  {  
    read->tag.protocol = (TMR_TagProtocol)GETU8(msg, *i);  
  }  
  if (flags & TMR_TRD_METADATA_FLAG_DATA)  
  {  
    int msgDataLen, readLen;  
    if (reader->continuousReading)  
    {  
      reader->u.serialReader.tagopSuccessCount = 1;  
    }  
  
    //Store data length in bits only.  
    read->data.len = GETU16(msg, *i);  
  
    //Convert data bit length into data byte length.  
    msgDataLen = tm_u8s_per_bits(read->data.len);  
    readLen = msgDataLen;  
  
    /**  
     * msgDataLen will be 0x1000(8000 / 8), when embedded tagOp fails.  
     * 0x8000 indicates the error, it is not the data length.  
     * Hence, no need to update the msgDataLen in this case!  
     */  
    if ((msgDataLen > read->data.max) && (msgDataLen != 0x1000))  
    {  
      return TMR_ERROR_OUT_OF_MEMORY;  
    }  
  
    if (NULL != read->data.list)  
    {  
#ifdef TMR_ENABLE_HF_LF  
      if (msgDataLen & 0x1000)  
      {  
        /**  
         * 0x8000 in msgDataLen field indicates an error when embedded tagOp fails.  
         * msgDataLen field indicates data length when embedded tagOp doesn’t fail. It is usually indicated in bits.  
         * So when embedded tagOp fails, it is equal to 0x1000 bytes (i.e., 0x8000 / 8) and it shouldn’t be  
         * considered as valid data length!  
         */  
        read->data.len = 0x8000;  
  
        /**  
         * If embedded tagOp fails, the data will contain error code of 2 bytes.  
         * Hence, making length 2.  
         */  
        msgDataLen = readLen = 2;  
      }  
#endif /* TMR_ENABLE_HF_LF */  
  
      memcpy(read->data.list, &msg[*i], msgDataLen);  
    }  
#ifdef TMR_ENABLE_UHF  
    /**  
    * if the gen2AllMemoryBankEnabled is enabled,  
    * extract the values  
    **/  
    if (reader->u.serialReader.gen2AllMemoryBankEnabled)  
    {  
      extractGen2MemoryBankValues(read);  
      /**  
      * Now, we extracted all the values,  
      * Disable the gen2AllMemoryBankEnabled option.  
      **/  
    }  
#endif /* TMR_ENABLE_UHF */  
    *i += readLen;  
  }  
#ifdef TMR_ENABLE_UHF  
  if (flags & TMR_TRD_METADATA_FLAG_GPIO_STATUS)  
  {  
    int j;  
    uint8_t gpioByte=GETU8(msg, *i);  
  
    switch(reader->u.serialReader.versionInfo.hardware[0])  
    {  
#ifdef TMR_ENABLE_UHF  
      case TMR_SR_MODEL_MICRO:  
        read->gpioCount = 2;  
        break;  
      case TMR_SR_MODEL_M6E:  
#endif /* TMR_ENABLE_UHF */  
#ifdef TMR_ENABLE_HF_LF  
      case TMR_SR_MODEL_M3E:  
#endif /* TMR_ENABLE_HF_LF */  
      default:  
        read->gpioCount = 4;  
        break;  
    }  
    for (j=0;j<read->gpioCount ;j++)   
    {  
      read->gpio[j].id = j+1;  
      /* GPO status */  
      read->gpio[j].high = (((gpioByte >> j) & 0x1)== 1);  
      /* GPI status returned in tag read metadata (upper nibble of byte(4-7))*/  
      read->gpio[j].bGPIStsTagRdMeta = (((gpioByte >> (j+4)) & 0x1) == 1);  
    }  
  }  
  if (TMR_TAG_PROTOCOL_GEN2 == read->tag.protocol)  
  {  
    if(flags & TMR_TRD_METADATA_FLAG_GEN2_Q)  
    {  
      read->u.gen2.q.u.staticQ.initialQ = GETU8(msg, *i);  
    }  
    if(flags & TMR_TRD_METADATA_FLAG_GEN2_LF)  
    {  
      uint8_t value;  
      value = GETU8(msg, *i);  
      switch(value)   
      {  
        case 0x00:  
        read->u.gen2.lf = TMR_GEN2_LINKFREQUENCY_250KHZ;  
        break;  
        case 0x02:  
        read->u.gen2.lf = TMR_GEN2_LINKFREQUENCY_320KHZ;  
        break;  
        case 0x04:  
        read->u.gen2.lf = TMR_GEN2_LINKFREQUENCY_640KHZ;  
        break;  
      }  
    }  
    if(flags & TMR_TRD_METADATA_FLAG_GEN2_TARGET)  
    {  
      uint8_t value;  
      value = GETU8(msg, *i);  
      switch(value)   
      {  
        case 0x00:  
        read->u.gen2.target = TMR_GEN2_TARGET_A;  
        break;  
        case 0x01:  
        read->u.gen2.target = TMR_GEN2_TARGET_B;  
        break;  
      }  
    }  
  }  
  if (flags & TMR_TRD_METADATA_FLAG_BRAND_IDENTIFIER)  
  {  
    memcpy(read->brandIdentifier, &(msg[*i]), 2);  
    *i += 2;  
  }  
#endif /* TMR_ENABLE_HF_LF */  
#ifdef TMR_ENABLE_HF_LF  
  if(flags & TMR_TRD_METADATA_FLAG_TAGTYPE)  
  {  
    uint8_t len = 1, tagtype[8];  
  
    len = parseEBVdata(msg, tagtype, i);  
    read->tagType = TMR_SR_convertFromEBV(tagtype, len);  
  }  
#endif /* TMR_ENABLE_HF_LF */  
  
  return ret;  
}
```

So, read count is included if the READCOUNT flag is set.

Same for rssi with RSSI flag.

Same for antenna id, frequency, timestamp, phase, protocol, and data length & data.

If no data was found, the length will be set to 0x1000, so that is what we need to check for.

Gpio byte is retrieved if that flag is set.

After that we get to the Gen2 specific metadata, like Q, LF and Target(A/B).

Lastly, brand identifier and tag type are included.

The TMR_SR_parseMetadataOnly function is used by several other functions:

1. TMR_SR_parseMetadataFromMessage
    1. TMR_SR_receiveAutonomousReading
2. TMR_SR_cmdGEN2ReadTagData (0x28)

TMR_SR_cmdGEN2ReadTagData does not have embedded data, it is concatenated to the end.

TMR_SR_receiveAutonomousReading uses TMR_SR_parseMetadataFromMessage.

This parser does not concatenate data, but the EPC.

So, we have two distinct responses for different types of reads.

We have 0x28 with data and 0x29 & 0x22 with EPC data.

0x29 is different in that it can contain multiple results.

We think this just loops.

We also inspect what is happening in TMR_SR_receiveAutonomousReading before TMR_SR_parseMetadataFromMessage.

Parsing responses for writing is relatively simple, we just need to look at the status bytes.

## **Communication**

The response format is pretty simple once we read the code.
The response format for all reads starts the same:

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|Header|1|FF|Required|
|Length|1|[length of message]|Required|
|Opcode|1|[TMR_SR_OpCode] (most likely 0x22, 0x28, or 0x29)|Required|
|Status|2|[Status]|Required|
|Info|...|[Future Use]|Required|
|Read Count|1|[integer]|Optional (READCOUNT flag)|
|RSSI|1|[integer(signal strength)|Optional (RSSI flag)|
|Antenna ID|1|[integer]|Optional (ANTENNAID flag)|
|Frequency|3|[integer]|Optional (FREQUENCY flag)|
|Timestamp|4|[timestamp]|Optional (TIMESTAMP flag)|
|Phase|2||Optional (PHASE flag)|
|Protocol|1||Optional (PROTOCOL flag)|
|Embedded Data Length|2|[Integer/status code]|Optional (DATA flag)|
|Embedded Data|^||Optional (DATA flag)|
|GPIO|1||Optional (GPIO flag)|
|Q|1||Optional (Q flag)|
|LF|1||Optional (LF flag)|
|Target|1||Optional (Target flag)|
|Brand Identifier|2||Optional(BRAND_IDENTIFIER flag)|
|Tag Type|1..5||Optional(TAGTYPE flag)|
|Data||[based on opcode]|Required|
|checksum|2|[checksum of message]|Required|
For 0x28, data is simply a stream of data.
For 0x22 and 0x29, data is defined as:

|Fieldname|Length|Possible Values|Required|
|---|---|---|---|
|EPC Length|2||Required|
|PC|2||Optional (Gen2 only)|
|XPC1|2||Optional (check in PC)|
|XPC2|2||Optional (check in PC)|
|EPC|||Required|
|EPC Checksum|2||Required|
|checksum|2|[checksum of message]|Required|
For a 0x29, Info is defined as:

| Fieldname         | Length | Possible Values               | Required |
| ----------------- | ------ | ----------------------------- | -------- |
| Metadata          | 2      | [TMR_TRD_MetadataFlag] (0x57) | Required |
| Number of results | 2      | [integer]                     | Required |

We found this out by trying, so the structure has not been verified in the source code.
Example from the 0x29 read:

```
FF 38 29 // header, length, opcode  
00 00 // status  
00 57 // metadata  
00 02 // nr of results  
2B // Readcount  
C6 // RSSI  
11 // antenna  
00 00 00 09  // timestamp  
05 // protocol  
00 80 // EPC length  
34 00 // PC  
E2 00 47 04 57 60 64 26 E3 01 01 0E   
C4 BB // CRC  
2F // readcount   
CC // RSSI   
11 // antenna ID   
00 00 00 0E // timestamp   
05 // protocol ID   
00 80 // 16 bytes  
30 00 // PC  
E2 00 42 0E 8C 50 60 17 04 4E 79 49 // EPC ID  
50 A7 // checksum
```

### **Implementation**
We change the parseResponse function in the library.
It now takes a readConfig as input. This readConfig has a the metadata flag we need.
Brand Identifier, EPC, and XPC1/2 are slightly challenging. Brand identifier is variable I think.
EPC can take any amount of bytes, but there is an upper bound. XPC needs to check certain bits in the PC.
We also create a response struct to carry all relevant information

# **Questions**
We still have some outstanding questions:
1. What is secure read access?
2. How does it work with high memory tags?
3. What are the Gen2 settings?

# **Changes to library and Conclusion**

We also have not implemented any **length checks.**
Currently does not support **tag type** metadata.

We have made the following changes.
**In startReading:**
1. We added a variable for the off-time, which allows us to control the overheating problem
2. We added filter support
Our **structs** provided for **ReadConfig** and **TagFilter** provide a bit of readability in the code.
In **writeData** and **writeEPCData**: we added support for filters.
The functions can still be called with the old invocation, we just set filter to NULL in that case.
The **constructFilterMsg** function creates a filter from the TagFilter struct given as a parameter.
In **constructReadTagIdMultipleMsg** and **constructReadTagDataMsg** we construct the messages for a 0x22 and 0x28 respectively.

There are still options that need to be researched.
For example, **locking** and making tags **untraceable** are currently not implemented.
There are also **authentication** options.
We also don't support parts of a bank.

# Verification
Verification of the library should be three steps:
1. Does the code compile
2. Verify outputs from URA with the outputs of our messages (disable sending here)
3. Run the code
Easiest way to verify this is to run the Test firmware. This firmware contains some requests and responses, but does not use the reader itself. Further debugging can be done with the reader writer firmware in example 10.
