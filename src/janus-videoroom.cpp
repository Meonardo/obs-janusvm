#include <obs-module.h>
#include <obs-frontend-api.h>

#include "janus-videoroom.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("janus-videoroom", "en-US")
OBS_MODULE_AUTHOR("Meonardo")

const char *obs_module_name(void)
{
	return "janus-videoroom";
}
const char *obs_module_description(void)
{
	return "push media stream to janus";
}

os_cpu_usage_info_t *_cpuUsageInfo;

bool obs_module_load(void)
{
	// Initialize the cpu stats
	_cpuUsageInfo = os_cpu_usage_info_start();

	blog(LOG_INFO, "[obs_module_load] Module loaded.");
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs_module_unload] Shutting down...");

	// Destroy the cpu stats
	os_cpu_usage_info_destroy(_cpuUsageInfo);

	blog(LOG_INFO, "[obs_module_unload] Finished shutting down.");
}

os_cpu_usage_info_t *GetCpuUsageInfo()
{
	return _cpuUsageInfo;
}
