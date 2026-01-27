#pragma once

#include <string>
#include <windows.h>

namespace fluxent {

inline std::wstring ToWide(const std::string &utf8) {
  if (utf8.empty())
    return std::wstring();

  int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  if (len <= 0)
    return std::wstring();

  std::wstring wtext;
  wtext.resize(len);
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wtext[0], len);
  wtext.resize(len - 1);
  return wtext;
}

inline std::string ToUtf8(const std::wstring &wide) {
  if (wide.empty())
    return std::string();

  int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0,
                                nullptr, nullptr);
  if (len <= 0)
    return std::string();

  std::string utf8;
  utf8.resize(len);
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], len, nullptr,
                      nullptr);
  utf8.resize(len - 1);
  return utf8;
}

} // namespace fluxent
