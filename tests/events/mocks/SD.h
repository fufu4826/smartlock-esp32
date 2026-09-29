#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

class File {
 public:
  File() = default;
  File(std::map<std::string, std::string>* files, const std::string& path,
       bool directory, const char* mode, size_t* failWriteAfter = nullptr)
      : files_(files), path_(path), directory_(directory), open_(true),
        cursor_(path), position_(0), writable_(mode && mode[0] != 'r'),
        failWriteAfter_(failWriteAfter) {
    if (!directory_ && files_) {
      auto found = files_->find(path_);
      if (found == files_->end() && writable_)
        found = files_->emplace(path_, std::string()).first;
      if (found == files_->end()) open_ = false;
      else if (mode && mode[0] == 'a') position_ = found->second.size();
    }
  }

  explicit operator bool() const { return open_; }
  bool isDirectory() const { return open_ && directory_; }
  const char* name() const { return path_.c_str(); }
  size_t size() const {
    if (!open_ || directory_ || !files_) return 0;
    const auto found = files_->find(path_);
    return found == files_->end() ? 0 : found->second.size();
  }
  size_t readBytes(char* output, size_t length) {
    if (!open_ || directory_ || !files_ || output == nullptr) return 0;
    const auto found = files_->find(path_);
    if (found == files_->end()) return 0;
    const size_t got = std::min(length, found->second.size() - position_);
    found->second.copy(output, got, position_);
    position_ += got;
    return got;
  }
  size_t write(const uint8_t* bytes, size_t length) {
    if (!open_ || directory_ || !writable_ || !files_ || bytes == nullptr) return 0;
    auto found = files_->find(path_);
    if (found == files_->end()) return 0;
    if (position_ > found->second.size()) return 0;
    size_t written = length;
    if (failWriteAfter_ && *failWriteAfter_ != static_cast<size_t>(-1)) {
      written = std::min(length, *failWriteAfter_);
      *failWriteAfter_ = static_cast<size_t>(-1);
    }
    found->second.replace(position_, written,
                          reinterpret_cast<const char*>(bytes), written);
    position_ += written;
    return written;
  }
  void flush() {}
  void close() { open_ = false; }
  File openNextFile() {
    if (!open_ || !directory_ || !files_) return File();
    auto it = files_->upper_bound(cursor_);
    while (it != files_->end()) {
      if (it->first.compare(0, path_.size() + 1, path_ + "/") != 0) return File();
      const std::string child = it->first.substr(path_.size() + 1);
      cursor_ = it->first;
      if (child.find('/') == std::string::npos)
        return File(files_, it->first, false, FILE_READ, failWriteAfter_);
      it = files_->upper_bound(cursor_);
    }
    return File();
  }

 private:
  std::map<std::string, std::string>* files_ = nullptr;
  std::string path_;
  bool directory_ = false;
  bool open_ = false;
  std::string cursor_;
  size_t position_ = 0;
  bool writable_ = false;
  size_t* failWriteAfter_ = nullptr;
};

class SDClass {
 public:
  bool exists(const char* path) const {
    if (!path) return false;
    if (std::string(path) == "/smartlock/outbox") return directoryExists_;
    return files_.find(path) != files_.end();
  }
  bool mkdir(const char* path) {
    if (!path || std::string(path) != "/smartlock/outbox") return false;
    directoryExists_ = true;
    return true;
  }
  File open(const char* path, const char* mode) {
    if (!path || !mode) return File();
    const std::string value(path);
    if (value == "/smartlock/outbox") {
      return directoryExists_ ? File(&files_, value, true, mode, &failWriteAfter_) : File();
    }
    if (value.compare(0, 18, "/smartlock/outbox/") != 0) return File();
    return File(&files_, value, false, mode, &failWriteAfter_);
  }
  bool remove(const char* path) {
    return path && files_.erase(path) == 1;
  }
  void clear() { files_.clear(); directoryExists_ = false; }
  void failNextWriteAfter(size_t bytes) { failWriteAfter_ = bytes; }
  void setFileContents(const std::string& path, const std::string& contents) {
    files_[path] = contents;
  }
  size_t fileCount() const { return files_.size(); }
  const std::map<std::string, std::string>& files() const { return files_; }

 private:
  std::map<std::string, std::string> files_;
  bool directoryExists_ = false;
  size_t failWriteAfter_ = static_cast<size_t>(-1);
};

extern SDClass SD;
