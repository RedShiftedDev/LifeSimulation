#pragma once

// Platform-specific includes
#if defined(__APPLE__) || defined(__MACH__)
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/task.h>
#elif defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <windows.h>
#pragma comment(lib, "pdh.lib")
#elif defined(__linux__)
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <unistd.h>
#endif

namespace SystemUtils {

struct CPUUsage {
  float applicationUsage; // CPU usage of this application (0-100%)
};

#if defined(__APPLE__) || defined(__MACH__)
//------------------------------------------------------------------------------------------
// macOS Implementations
//------------------------------------------------------------------------------------------

[[nodiscard]] float getApplicationMemoryUsage() noexcept {
  struct task_basic_info info;
  mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;

  if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size) != KERN_SUCCESS) {
    return -1.0F;
  }

  return static_cast<float>(info.resident_size) / (1024 * 1024);
}

[[nodiscard]] float getApplicationCPUUsage() noexcept {
  task_info_data_t tinfo;
  mach_msg_type_number_t task_info_count = TASK_INFO_MAX;

  if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)tinfo, &task_info_count) != KERN_SUCCESS) {
    return -1.0F;
  }

  thread_array_t thread_list;
  mach_msg_type_number_t thread_count;

  if (task_threads(mach_task_self(), &thread_list, &thread_count) != KERN_SUCCESS) {
    return -1.0F;
  }

  float total_cpu = 0;
  for (unsigned int i = 0; i < thread_count; i++) {
    thread_info_data_t thinfo;
    mach_msg_type_number_t thread_info_count = THREAD_INFO_MAX;

    if (thread_info(thread_list[i], THREAD_BASIC_INFO, (thread_info_t)thinfo, &thread_info_count) == KERN_SUCCESS) {
      thread_basic_info_t basic_info_th = (thread_basic_info_t)thinfo;
      if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
        total_cpu += basic_info_th->cpu_usage / (float)TH_USAGE_SCALE * 100.0;
      }
    }
  }

  vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
  return total_cpu;
}

#elif defined(_WIN32) || defined(_WIN64)
//------------------------------------------------------------------------------------------
// Windows Implementations
//------------------------------------------------------------------------------------------

[[nodiscard]] float getApplicationMemoryUsage() noexcept {
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
    return -1.0F;
  }
  return static_cast<float>(pmc.WorkingSetSize) / (1024 * 1024);
}

[[nodiscard]] float getApplicationCPUUsage() noexcept {
  static PDH_HQUERY query = nullptr;
  static PDH_HCOUNTER counter = nullptr;
  static bool initialized = false;

  if (!initialized) {
    if (PdhOpenQuery(nullptr, 0, &query) != ERROR_SUCCESS) {
      return -1.0F;
    }

    WCHAR counterPath[PDH_MAX_COUNTER_PATH];
    WCHAR procNameBuffer[MAX_PATH];
    if (GetModuleBaseNameW(GetCurrentProcess(), nullptr, procNameBuffer, MAX_PATH) > 0) {
      std::wstring path = L"\\Process(" + std::wstring(procNameBuffer) + L")\\% Processor Time";
      if (PdhAddCounterW(query, path.c_str(), 0, &counter) != ERROR_SUCCESS) {
        PdhCloseQuery(query);
        return -1.0F;
      }
      PdhCollectQueryData(query);
      initialized = true;
    }
  }

  if (initialized) {
    PDH_FMT_COUNTERVALUE value;
    PdhCollectQueryData(query);
    if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
      return static_cast<float>(value.doubleValue);
    }
  }
  return -1.0F;
}

#elif defined(__linux__)
//------------------------------------------------------------------------------------------
// Linux Implementations
//------------------------------------------------------------------------------------------

[[nodiscard]] float getApplicationMemoryUsage() noexcept {
  std::string statusFile = "/proc/" + std::to_string(getpid()) + "/status";
  std::ifstream file(statusFile);
  std::string line;
  unsigned long vmRSS = 0;

  if (file.is_open()) {
    while (std::getline(file, line)) {
      if (line.find("VmRSS:") != std::string::npos) {
        std::string value = line.substr(line.find(":") + 1);
        value.erase(value.find_last_not_of(" \t\n\r\fkB") + 1);
        value.erase(0, value.find_first_not_of(" \t\n\r\f"));
        try {
          vmRSS = std::stoul(value);
        } catch (...) {
          return -1.0F;
        }
        break;
      }
    }
  }
  return static_cast<float>(vmRSS) / 1024.0F;
}

[[nodiscard]] float getApplicationCPUUsage() noexcept {
  static unsigned long long lastProcessUserTime = 0, lastProcessSystemTime = 0;
  static auto lastTime = std::chrono::high_resolution_clock::now();

  std::ifstream statFile("/proc/" + std::to_string(getpid()) + "/stat");
  std::string line;
  if (std::getline(statFile, line)) {
    std::istringstream iss(line);
    std::string unused;
    unsigned long long utime, stime;
    
    for (int i = 1; i < 14; ++i) iss >> unused;
    iss >> utime >> stime;

    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count();

    if (elapsedTime > 0 && lastProcessUserTime > 0) {
      unsigned long long totalTime = (utime + stime) - (lastProcessUserTime + lastProcessSystemTime);
      float percent = static_cast<float>(totalTime * 10000.0 / (elapsedTime * sysconf(_SC_CLK_TCK)));
      
      lastProcessUserTime = utime;
      lastProcessSystemTime = stime;
      lastTime = currentTime;
      
      return percent;
    }

    lastProcessUserTime = utime;
    lastProcessSystemTime = stime;
    lastTime = currentTime;
  }
  return -1.0F;
}

#else
//------------------------------------------------------------------------------------------
// Fallback Implementations
//------------------------------------------------------------------------------------------

[[nodiscard]] float getApplicationMemoryUsage() noexcept {
  return -1.0F;
}

[[nodiscard]] float getApplicationCPUUsage() noexcept {
  return -1.0F;
}

#endif

} // namespace SystemUtils
