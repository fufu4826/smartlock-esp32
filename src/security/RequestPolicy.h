#pragma once
#include <stddef.h>
#include <string.h>

namespace RequestPolicy {
inline bool deviceId(const char* id) {
  if (!id || strlen(id) != 7 || id[0] != 'D' || !strcmp(id, "D000000")) return false;
  for (size_t i = 1; i < 7; ++i) if (id[i] < '0' || id[i] > '9') return false;
  return true;
}
inline char lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a'-'A') : c; }
inline bool word(const char*& p, const char* expected) {
  for (; *expected; ++expected, ++p) if (lower(*p) != *expected) return false;
  return true;
}
inline void spaces(const char*& p) { while (*p == ' ' || *p == '\t') ++p; }
inline bool formContentType(const char* p) {
  if (!p) return false;
  spaces(p);
  if (!word(p, "application/x-www-form-urlencoded")) return false;
  spaces(p);
  if (!*p) return true;
  if (*p++ != ';') return false;
  spaces(p);
  if (!word(p, "charset")) return false;
  spaces(p);
  if (*p++ != '=') return false;
  spaces(p);
  const bool quoted = *p == '"'; if (quoted) ++p;
  if (!word(p, "utf-8")) return false;
  if (quoted && *p++ != '"') return false;
  spaces(p);
  return !*p;
}
// Strict Unicode scalar values, rejecting overlong encodings, surrogate code
// points, controls and unterminated fields. SSIDs remain byte bounded.
inline bool utf8Text(const char* p, size_t capacity) {
  if (!p || !capacity) return false;
  size_t used = 0;
  while (used < capacity && p[used]) {
    unsigned c = static_cast<unsigned char>(p[used++]);
    if (c < 0x20 || c == 0x7f) return false;
    if (c < 0x80) continue;
    unsigned more, value, minimum;
    if (c >= 0xc2 && c <= 0xdf) { more=1; value=c&31; minimum=0x80; }
    else if (c >= 0xe0 && c <= 0xef) { more=2; value=c&15; minimum=0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { more=3; value=c&7; minimum=0x10000; }
    else return false;
    while (more--) {
      if (used >= capacity) return false;
      c=static_cast<unsigned char>(p[used++]);
      if ((c&0xc0)!=0x80) return false;
      value=(value<<6)|(c&63);
    }
    if (value < minimum || value > 0x10ffff || (value>=0xd800&&value<=0xdfff)) return false;
  }
  return used > 0 && used < capacity;
}
}
