#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <initializer_list>

namespace LineProtocol {
// Bounded structural parser. No allocation and no interpretation as local commands.
class Json {
 public:
  explicit Json(const char* text): p_(text) {}
  bool object() { space(); if(*p_!='{')return false; return value(0) && (space(),!*p_); }
 private:
  const char* p_;
  void space(){while(*p_==' '||*p_=='\t'||*p_=='\r'||*p_=='\n')++p_;}
  bool string(){
    if(*p_++!='"')return false;
    while(*p_){unsigned char c=*p_++;if(c=='"')return true;if(c<32)return false;
      if(c=='\\'){char e=*p_++;if(!e)return false;if(e=='u'){
        for(int i=0;i<4;++i){char h=*p_++;if(!h||!((h>='0'&&h<='9')||(h>='a'&&h<='f')||(h>='A'&&h<='F')))return false;}
      }else if(!strchr("\"\\/bfnrt",e))return false;}
    }return false;
  }
  bool number(){
    if(*p_=='-')++p_;if(*p_=='0')++p_;else{if(*p_<'1'||*p_>'9')return false;while(*p_>='0'&&*p_<='9')++p_;}
    if(*p_=='.'){++p_;if(*p_<'0'||*p_>'9')return false;while(*p_>='0'&&*p_<='9')++p_;}
    if(*p_=='e'||*p_=='E'){++p_;if(*p_=='+'||*p_=='-')++p_;if(*p_<'0'||*p_>'9')return false;while(*p_>='0'&&*p_<='9')++p_;}return true;
  }
  bool value(unsigned depth){
    if(depth>8)return false;space();if(*p_=='"')return string();
    if(*p_=='{'||*p_=='['){bool obj=*p_=='{';char end=obj?'}':']';++p_;space();if(*p_==end){++p_;return true;}
      for(;;){if(obj){if(*p_!='"'||!string())return false;space();if(*p_++!=':')return false;}
        if(!value(depth+1))return false;space();if(*p_==end){++p_;return true;}if(*p_++!=',')return false;space();}
    }
    for(const char* word:{"true","false","null"}){size_t n=strlen(word);if(!strncmp(p_,word,n)){p_+=n;return true;}}
    return number();
  }
};
inline bool jsonObject(const char* body){return body && Json(body).object();}
inline bool decimal(const char* p,uint32_t& value){
  if(!p||*p<'0'||*p>'9')return false;value=0;
  while(*p>='0'&&*p<='9'){unsigned d=*p++-'0';if(value>(UINT32_MAX-d)/10)return false;value=value*10+d;}
  while(*p==' '||*p=='\t')++p;return !*p;
}
inline bool numberField(const char* body,const char* key,uint16_t& out){
  char needle[40];size_t k=strlen(key);if(k>35)return false;needle[0]='"';memcpy(needle+1,key,k);needle[k+1]='"';needle[k+2]=0;
  const char* p=strstr(body,needle);if(!p)return false;p+=k+2;while(*p==' '||*p=='\t')++p;if(*p++!=':')return false;
  while(*p==' '||*p=='\t')++p;if(*p<'0'||*p>'9')return false;uint32_t v=0;
  while(*p>='0'&&*p<='9'){v=v*10+(*p++-'0');if(v>65535)return false;}while(*p==' '||*p=='\t')++p;
  if(*p!=','&&*p!='}')return false;out=v;return true;
}
inline bool acceptedId(const char* p){
  while(*p==' '||*p=='\t')++p;size_t n=0;
  while(*p){unsigned char c=*p++;if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;if(++n>128)return false;}
  return n>0;
}
inline bool limitedQuota(const char* body){
  const char* p=strstr(body,"\"type\"");if(!p)return false;p+=6;
  while(*p==' '||*p=='\t')++p;if(*p++!=':')return false;
  while(*p==' '||*p=='\t')++p;return !strncmp(p,"\"limited\"",9);
}
}
