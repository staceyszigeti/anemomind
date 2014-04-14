#include "string.h"
#include <assert.h>
#include <stdarg.h>
#include <algorithm>
#include <server/common/endianess.h>

namespace sail {

bool isBlank(char c) {
  return c == ' ' || c == '\n';
}

bool notIsBlank(char c) {
  return !isBlank(c);
}

bool tryParseInt(const std::string &s, int &out) {
  try {
    out = std::stoi(s);
    return true;
  } catch (std::exception &e) {
    return false;
  }
}

bool tryParseDouble(const std::string &s, double &out) {
  try {
    out = std::stod(s);
    return true;
  } catch (std::exception &e) {
    return false;
  }
}


// SEE http://en.cppreference.com/w/cpp/language/escape
bool isEscaped(char c) {
  return c == '\'' ||
         c == '\"' ||
         c == '\?' ||
         c == '\\' ||
         c == '\0' ||
         c == '\a' ||
         c == '\b' ||
         c == '\f' ||
         c == '\n' ||
         c == '\r' ||
         c == '\t' ||
         c == '\v';
}

#define GES(a, b) if (c == (a)) {return b;}

std::string getEscapeString(char c) {
  GES('\'', "\\\'");
  GES('\"', "\\\"");
  GES('\?', "\\\?");
  GES('\\', "\\\\");
  GES('\0', "\\\0");
  GES('\a', "\\\a");
  GES('\b', "\\\b");
  GES('\f', "\\\f");
  GES('\n', "\\\n");
  GES('\r', "\\\r");
  GES('\t', "\\\t");
  GES('\v', "\\\v");
  return "";
}

char toHexDigit(int value) {
  assert(0 <= value);
  assert(value < 16);
  static const char digits[17] = "0123456789ABCDEF";
  return digits[value];
}

unsigned char decodeHexDigit(char c) {
  if ('A' <= c && c <= 'F') {
    return (c - 'A') + 10;
  } else if ('a' <= c && c <= 'f') {
    return (c - 'a') + 10;
  } else if ('0' <= c && c <= '9') {
    return c - '0';
  } else {
    return 255;
  }
}

bool isHexDigit(char c) {
  return decodeHexDigit(c) != 255;
}

bool areHexDigits(int count, const char *c) {
  for (int i = 0; i < count; i++) {
    if (!isHexDigit(c[i])) {
      return false;
    }
  }
  return true;
}

bool isHexString(const std::string &s, int expectedLength) {
  if (expectedLength == -1 || s.length() == expectedLength) {
    return areHexDigits(s.length(), s.c_str());
  }
  return false;
}

std::string bytesToHex(size_t n, uint8_t *bytes) {
  std::string dst(2*n, '0');

  for (int i = 0; i < n; i++) {
    int offs = 2*i;
    uint8_t b = bytes[i];
    dst[offs + 0] = toHexDigit(b/16);
    dst[offs + 1] = toHexDigit(b % 16);
  }

  return dst;
}

std::string formatInt(const std::string &fstr, int value) {
  char dst[255];
  if (sprintf(dst, fstr.c_str(), value) >= 0) {
    return std::string(dst);
  } else {
    return "";
  }
}

std::string stringFormat(const std::string &fmt, ...) {
  // http://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
  int size = 100;
  std::string str;
  va_list ap;
  while (1) {
    str.resize(size);
    va_start(ap, fmt);
    int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
    va_end(ap);
    if (n > -1 && n < size) {
      str.resize(n);
      return str;
    }
    if (n > -1)
      size = n + 1;
    else
      size *= 2;
  }
  return str;
}


//std::string data = "Abc";
void toLowerInPlace(std::string &data) {
  std::transform(data.begin(), data.end(), data.begin(), ::tolower);
}

std::string toLower(const std::string &src) {
  std::string dst = src;
  toLowerInPlace(dst);
  return dst;
}


namespace {

  int64_t reverse(int64_t x) {
    int64_t y;
    uint8_t *xb = (uint8_t *)(&x);
    uint8_t *yb = (uint8_t *)(&y);
    constexpr int count = sizeof(x);
    for (int i = 0; i < count; i++) {
      yb[i] = xb[count-1-i];
    }
    return y;
  }

  std::string int64ToHexLittleEndian(int64_t x) {
    return bytesToHex(sizeof(x), (uint8_t *)(&x));
  }
}

//std::string int64ToHex(int64_t x) {
//  if (isBigEndian()) {
//    return int64ToHexLittleEndian(x);
//  }
//  return int64ToHexLittleEndian(reverse(x));
//}

std::string int64ToHex(int64_t x) {
  constexpr int len = 2*sizeof(x);
  std::string result(len, '0');
  for (int i = 0; i < sizeof(x); ++i) {
    int offs = 2*i;
    result[offs + 0] = toHexDigit((x >> (15 - i) * 8 + 4) & 0xf);
    result[offs + 1] = toHexDigit((x >> (15 - i) * 8 ) & 0xf);
  }
  return result;
}

void splitFilenamePrefixSuffix(const std::string &filename,
                               std::string &prefix, std::string &suffix) {
  int index = filename.find_last_of('.');
  if (index < filename.length()) {
    prefix = filename.substr(0, index);
    int n = filename.length() - (index + 1);
    suffix = filename.substr(index+1, n);
  } else {
    prefix = filename;
    suffix = "";
  }
}

} /* namespace sail */
