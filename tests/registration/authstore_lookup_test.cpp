#include "AuthStore.h"
#include "AtomicFileStore.h"
#include <mbedtls/sha256.h>
#include <cassert>
#include <cstring>
#include <iostream>

namespace {
uint8_t payload[AtomicFileStore::kMaxPayload] = {};
size_t payloadLength = 0;
AtomicFileStore::ReadResult readResult = AtomicFileStore::ReadResult::CurrentValid;
size_t hashCalls = 0;
struct TestHeader { uint32_t magic, schema, count; };
struct TestEntry { char id[9]; uint8_t salt[16]; uint8_t hash[32]; };

void hashEntry(TestEntry& entry, const char* credential) {
  uint8_t input[48] = {};
  memcpy(input, entry.salt, 16);
  for (size_t i=0;i<32;++i) {
    const char c=credential[i*2];
    const char d=credential[i*2+1];
    auto nibble=[](char x)->uint8_t{return x<='9'?static_cast<uint8_t>(x-'0'):static_cast<uint8_t>(x-'a'+10);};
    input[16+i]=static_cast<uint8_t>((nibble(c)<<4)|nibble(d));
  }
  assert(mbedtls_sha256_ret(input,sizeof(input),entry.hash,0)==0);
}

void installEntries(const TestEntry* entries,size_t count) {
  memset(payload,0,sizeof(payload));
  TestHeader header{0x48545541u,2,static_cast<uint32_t>(count)};
  memcpy(payload,&header,sizeof(header));
  memcpy(payload+sizeof(header),entries,count*sizeof(TestEntry));
  payloadLength=sizeof(header)+count*sizeof(TestEntry);
  readResult=AtomicFileStore::ReadResult::CurrentValid;
}
}

AtomicFileStore::ReadResult AtomicFileStore::read(const char*,uint8_t* output,size_t capacity,size_t& length) {
  length=0;
  if(readResult!=ReadResult::CurrentValid)return readResult;
  if(capacity<payloadLength)return ReadResult::Corrupt;
  memcpy(output,payload,payloadLength);length=payloadLength;return ReadResult::CurrentValid;
}
bool AtomicFileStore::write(const char*,const uint8_t*,size_t){return false;}
bool AtomicFileStore::repairUnconfiguredSetupFile(const char*){return false;}

int mbedtls_sha256_ret(const unsigned char* input,size_t length,unsigned char output[32],int) {
  ++hashCalls;
  for(size_t i=0;i<32;++i)output[i]=static_cast<uint8_t>(0x5a+i*13);
  for(size_t i=0;i<length;++i){size_t slot=i%32;output[slot]=static_cast<uint8_t>((output[slot]*33u)^input[i]^static_cast<uint8_t>(i));}
  return 0;
}

int main() {
  TestEntry entries[2] = {};
  memcpy(entries[0].id,"D000001",8); memcpy(entries[1].id,"D000002",8);
  for(size_t i=0;i<16;++i){entries[0].salt[i]=static_cast<uint8_t>(i+1);entries[1].salt[i]=static_cast<uint8_t>(0xa0+i);}
  const char* first="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  const char* second="fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
  hashEntry(entries[0],first);hashEntry(entries[1],second);installEntries(entries,2);
  char id[9]="stale";
  hashCalls=0;
  assert(AuthStore::findDeviceByCredential(second,id));
  assert(!strcmp(id,"D000002"));
  assert(hashCalls==2);
  hashCalls=0;
  assert(!AuthStore::findDeviceByCredential("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",id));
  assert(id[0]=='\0');
  assert(hashCalls==2);
  assert(!AuthStore::findDeviceByCredential("ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789",id));
  TestEntry duplicate[2]= {entries[0],entries[1]};
  hashEntry(duplicate[1],first);installEntries(duplicate,2);
  assert(!AuthStore::findDeviceByCredential(first,id));
  assert(id[0]=='\0');
  readResult=AtomicFileStore::ReadResult::Corrupt;
  assert(!AuthStore::findDeviceByCredential(second,id));
  assert(id[0]=='\0');
  std::cout<<"PASS: production AuthStore salted verifier lookup, full scan, malformed input, ambiguity and corrupt-store rejection (11 assertions)\n";
}
