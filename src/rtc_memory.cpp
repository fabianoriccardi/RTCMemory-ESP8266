#include "rtc_memory.h"
#include <FS.h>

bool RtcMemory::begin(){
  if(!ready){
    if(verbosity > 1) Serial.print("Loading RTC memory... ");

    //In this case, I have to verify the memory crc.
    //If not verified, load the default value from the flash.
    if(ESP.rtcUserMemoryRead(0, (uint32_t*) &rtcData, sizeof(rtcData))){
      uint32_t crcOfData = calculateCRC32(((uint8_t*) &rtcData) + 4, sizeof(rtcData) - 4);
      if(verbosity > 1){
        Serial.print("CRC32 of data: ");
        Serial.println(crcOfData, HEX);
        Serial.print("CRC32 read from RTC: ");
        Serial.println(rtcData.crc32, HEX);
      }
      if (crcOfData != rtcData.crc32) {
        if(verbosity > 0) Serial.print("CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid! reading from flash..");
        if(readFromFlash()){
          if(verbosity > 1) Serial.println("Loading backup from FLASH ok");
        }else{
          if(verbosity > 0) Serial.println("Loading backup from FLASH NOT ok, data are loaded and resetted");
        }
      }else {
        if(verbosity > 1) Serial.println("The CRC is correct, the data are probably correct");
      }
    }else{
      if(verbosity > 0) Serial.println("Read RTC memory failure");
      return false;
    }
    ready=true;
    if(verbosity > 1) Serial.println("Done!");
  }else{
    if(verbosity > 1) Serial.println("Rtc Memory already loaded");
  }
  return true;
}

bool RtcMemory::save(){
  if(ready){
    //update the crc32
    uint32_t crcOfData = calculateCRC32(((uint8_t*) &rtcData) + 4, sizeof(rtcData) - 4);
    rtcData.crc32=crcOfData;
    
    if(ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData))){
      if(verbosity > 1) Serial.println("Write to RtcMemory done");
      return true;
    }else{
      if(verbosity > 0) Serial.println("Write to RtcMemory NOT done");
      return false;
    }
  }else{
    Serial.println("Call init before other calls!");
    return false;
  }
}

bool RtcMemory::persist(){
  if(ready){
    bool res=save();
    if(res){
      return writeToFlash();
    }
  }else{
    if(verbosity > 0) Serial.println("Call init before other calls!");
  }
  return false;
}

byte* RtcMemory::getRtcData(){
  if(ready){
    return rtcData.data;
  }
  if(verbosity > 0) Serial.println("Call init before other calls!");
  return nullptr;
}

RtcMemory::RtcMemory(String path, int verbosity):
                      filePath(path),ready(false),verbosity(verbosity)
{  
}

bool RtcMemory::readFromFlash(){
  if(verbosity > 1) Serial.print(String("Setting ") + filePath + "... ");
  //check if the file exists
  File f;  
  if(SPIFFS.exists(filePath)){
    f = SPIFFS.open(filePath,"r");
    if(f!=NULL){
      int byteRead = f.read((uint8_t*)&rtcData,dataLength+4);
      if(verbosity > 1) Serial.println(String("Bytes read:") + byteRead);
      f.close();
      if(byteRead!=dataLength+4){
        memoryReset();
        writeToFlash(); 
        return false;
      }
    }else{
      if(verbosity > 0) Serial.println(String("Error in opening: ")+ filePath);
      memoryReset();
      writeToFlash();    
      return false;
    }
  }else{
    if(verbosity > 0) Serial.println("File not existing");
    memoryReset();  
    writeToFlash();
    return false;
  }
  return true;
}

bool RtcMemory::writeToFlash(){
  File f = SPIFFS.open(filePath,"w");
  if(f!=NULL){
    f.write((uint8_t*)&rtcData,dataLength+4);
    f.close();
    return true;
  }else{
    if(verbosity >0) Serial.println("Error in writing the flash. Is the filesystem initialized?");
    return false;
  }
}

uint32_t RtcMemory::calculateCRC32(const uint8_t *data, size_t length){
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

void RtcMemory::memoryReset(){
  //Just to reset the RAM memory
  for(int i=0;i<sizeof(RtcData)/sizeof(unsigned int);i++){
    ((unsigned int*)&rtcData)[i]=0;
  }
  uint32_t result=calculateCRC32((uint8_t*)rtcData.data,dataLength);
  rtcData.crc32=result;
}

