// system_utils.h
#pragma once

#include <mach/mach.h>
#include <mach/task.h>

namespace SystemUtils {

[[nodiscard]] float getApplicationMemoryUsage() noexcept {
  struct task_basic_info info;
  mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;

  if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size) != KERN_SUCCESS) {
    return -1.0F;
  }

  // Return memory usage in megabytes
  return static_cast<float>(info.resident_size) / (1024 * 1024);
}

} // namespace SystemUtils
