#include "fluxent/debug_log.hpp"
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <Windows.h>

namespace fluxent {

static std::mutex g_debug_log_mutex;

void DebugLog(const char *msg)
{
  // Get temp directory
  char temp_path[MAX_PATH] = {0};
  DWORD len = GetTempPathA(MAX_PATH, temp_path);
  if (len == 0 || len > MAX_PATH) {
    return;
  }

  // Build full log path: %TEMP%\fluxent_debug.log
  std::string log_file = temp_path;
  if (!log_file.empty() && log_file.back() != '\\') {
    log_file += '\\';
  }
  log_file += "fluxent_debug.log";

  std::lock_guard<std::mutex> lk(g_debug_log_mutex);
  std::ofstream f(log_file, std::ios::app);
  if (!f)
    return;

  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf;
  localtime_s(&tm_buf, &t);
  f << std::put_time(&tm_buf, "%F %T") << " " << msg << '\n';
}

} // namespace fluxent
