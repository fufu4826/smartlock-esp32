#include "RequestPolicy.h"

#include <cstdlib>
#include <iostream>

namespace {
unsigned checks = 0;
void check(bool condition, const char* description) {
  ++checks;
  if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}

void contentTypes() {
  check(RequestPolicy::formContentType("application/x-www-form-urlencoded"),
        "bare form media type is accepted");
  check(RequestPolicy::formContentType("Application/X-Www-Form-Urlencoded"),
        "media type comparison is case insensitive");
  check(RequestPolicy::formContentType("application/x-www-form-urlencoded; charset=utf-8"),
        "UTF-8 charset parameter is accepted");
  check(RequestPolicy::formContentType(" application/x-www-form-urlencoded ; charset = UTF-8 \t"),
        "optional whitespace around exact media type and charset is accepted");

  const char* rejected[] = {
      nullptr,
      "",
      "application/json",
      "application/x-www-form-urlencodedx",
      "x-application/x-www-form-urlencoded",
      "application/x-www-form-urlencoded.evil",
      "application/x-www-form-urlencoded; charset=latin1",
      "application/x-www-form-urlencoded; charset=utf-8; boundary=x",
      "application/x-www-form-urlencoded; charset=utf-8, application/json",
      "application/x-www-form-urlencoded\r\nX-Injected: yes",
  };
  for (const char* value : rejected)
    check(!RequestPolicy::formContentType(value), "malformed or non-form content type is rejected");
}

void deviceIds() {
  check(RequestPolicy::deviceId("D000001"), "Owner device ID is accepted");
  check(RequestPolicy::deviceId("D123456"), "six-digit device ID is accepted");
  const char* rejected[] = {
      nullptr, "", "D000000", "D00001", "D0000001", "d000001", "X000001",
      "D00000A", "D000001x", " D000001", "D000001 ", "DD00001",
  };
  for (const char* value : rejected)
    check(!RequestPolicy::deviceId(value), "malformed, zero, or noncanonical device ID is rejected");
}

void utf8Fields() {
  check(RequestPolicy::utf8Text("SmartLock", 32), "ASCII UTF-8 text is accepted");
  check(RequestPolicy::utf8Text("ชื่อ", 32), "Thai UTF-8 text is accepted");
  check(!RequestPolicy::utf8Text(nullptr, 8), "null UTF-8 input is rejected");
  check(!RequestPolicy::utf8Text("x", 0), "zero-capacity UTF-8 field is rejected");
  check(!RequestPolicy::utf8Text("", 8), "empty UTF-8 field is rejected");
  check(!RequestPolicy::utf8Text("bad\ntext", 16), "UTF-8 field with a control character is rejected");
  check(!RequestPolicy::utf8Text("bad\x7f", 16), "UTF-8 field with DEL is rejected");
  check(!RequestPolicy::utf8Text("\xc0\xaf", 8), "overlong UTF-8 encoding is rejected");
  check(!RequestPolicy::utf8Text("\x80", 8), "isolated continuation byte is rejected");
  check(!RequestPolicy::utf8Text("\xe0\x80\x80", 8), "overlong three-byte sequence is rejected");
  check(!RequestPolicy::utf8Text("\xed\xa0\x80", 8), "UTF-8 encoded surrogate is rejected");
  check(!RequestPolicy::utf8Text("\xf4\x90\x80\x80", 8), "code point above U+10FFFF is rejected");
  check(!RequestPolicy::utf8Text("\xe2\x28\xa1", 8), "invalid continuation byte is rejected");
  check(!RequestPolicy::utf8Text("\xe2\x82", 8), "truncated multibyte sequence is rejected");
  const char noTerminator[] = {'a', 'b'};
  check(!RequestPolicy::utf8Text(noTerminator, sizeof(noTerminator)), "unterminated bounded field is rejected");
}
}

int main() {
  contentTypes();
  deviceIds();
  utf8Fields();
  std::cout << "PASS: " << checks << " RequestPolicy assertions\n";
}
