#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <set>
#include <utility>

#define FILE_READ "r"
#define FILE_WRITE "w"

struct SDMutationState {
  size_t remaining=0;
  size_t count=0;
  bool enabled=false;
  bool triggered=false;
  bool beforeMutation(){
    ++count;
    if(!enabled)return false;
    if(remaining==0){enabled=false;triggered=true;return true;}
    --remaining;return false;
  }
};

class File {
 public:
  File() = default;
  File(std::map<std::string,std::string>* files,const std::string& path,const char* mode,SDMutationState* mutations)
      : files_(files),path_(path),mutations_(mutations),open_(files!=nullptr),writable_(mode&&mode[0]!='r') {
    if (!open_) return;
    auto it=files_->find(path_);
    if (it==files_->end()&&writable_) it=files_->emplace(path_,std::string()).first;
    if (it==files_->end()) open_=false;
    if (it!=files_->end()&&mode&&mode[0]=='a') position_=it->second.size();
  }
  explicit operator bool() const { return open_; }
  size_t size() const {if(!open_||!files_)return 0;auto it=files_->find(path_);return it!=files_->end()?it->second.size():0;}
  size_t readBytes(char* out,size_t count) {if(!open_||!out)return 0;auto it=files_->find(path_);if(it==files_->end())return 0;size_t n=std::min(count,it->second.size()-std::min(position_,it->second.size()));it->second.copy(out,n,position_);position_+=n;return n;}
  size_t read(uint8_t* out,size_t count) {return readBytes(reinterpret_cast<char*>(out),count);}
  size_t write(const uint8_t* bytes,size_t count) {if(!open_||!writable_||!bytes)return 0;if(mutations_&&mutations_->beforeMutation())return 0;auto it=files_->find(path_);if(it==files_->end()||position_>it->second.size())return 0;it->second.replace(position_,count,reinterpret_cast<const char*>(bytes),count);position_+=count;return count;}
  void flush() {}
  void close(){open_=false;}
 private:
  std::map<std::string,std::string>* files_=nullptr;
  std::string path_;
  SDMutationState* mutations_=nullptr;
  bool open_=false,writable_=false;
  size_t position_=0;
};

class SDClass {
 public:
  bool exists(const char* p) const {return p&&(files_.count(p)||dirs_.count(p));}
  bool mkdir(const char* p) {if(!p||mutations_.beforeMutation())return false;dirs_.insert(p);return true;}
  File open(const char* p,const char* mode) {
    if(!p||!mode||dirs_.count(p))return File();
    const bool writing=mode[0]!='r';
    if(writing&&mutations_.beforeMutation())return File();
    return File(&files_,p,mode,&mutations_);
  }
  bool remove(const char* p){if(!p||mutations_.beforeMutation())return false;return files_.erase(p)==1;}
  bool rename(const char* a,const char* b){if(!a||!b||mutations_.beforeMutation())return false;auto it=files_.find(a);if(it==files_.end()||files_.count(b))return false;files_[b]=it->second;files_.erase(it);return true;}
  // Allow n mutations, then fail exactly the next attempted mutation once.
  void failAfterMutations(size_t n){mutations_.remaining=n;mutations_.enabled=true;mutations_.triggered=false;mutations_.count=0;}
  bool faultTriggered()const{return mutations_.triggered;}
  size_t mutationCount()const{return mutations_.count;}
  void clear(){files_.clear();dirs_.clear();mutations_=SDMutationState{};}
  void setFileContents(const std::string& path,const std::string& data){files_[path]=data;}
  std::string fileContents(const std::string& path) const {auto it=files_.find(path);return it==files_.end()?std::string():it->second;}
 private:
  std::map<std::string,std::string> files_;
  std::set<std::string> dirs_;
  SDMutationState mutations_{};
};
extern SDClass SD;

