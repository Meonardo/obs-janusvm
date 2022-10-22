#pragma once

#include <obs.hpp>
#include <util/platform.h>

#define blog(level, msg, ...) blog(level, "[janus-videoroom] " msg, ##__VA_ARGS__)

os_cpu_usage_info_t *GetCpuUsageInfo();
