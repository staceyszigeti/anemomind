#include "string.h"
#include <assert.h>
#include <stdarg.h>
#include <algorithm>

namespace sail {

bool isBlank(char c) {
  return c == ' ' || c == '\n';
}

bool notIsBlank(char c) {
  return !isBlank(c);
}

bool tryParseInt(std::string s, int &out) {
  try {
    out = std::stoi(s);
    return true;
  } catch (std::exception &e) {
    return false;
  }
}

bool tryParseDouble(std::string s, double &out) {
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

std::string formatInt(std::string fstr, int value) {
  char dst[255];
  if (sprintf(dst, fstr.c_str(), value) >= 0) {
    return std::string(dst);
  } else {
    return "";
  }
}

std::string stringFormat(const std::string fmt, ...) {
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

std::string toLower(std::string src) {
  std::string dst = src;
  toLowerInPlace(dst);
  return dst;
}



void splitFilenamePrefixSuffix(std::string filename,
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
