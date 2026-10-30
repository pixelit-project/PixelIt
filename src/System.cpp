#include "FirmwareState.h"
#include "Tools.h"

String ResetReason()
{
#if defined(ESP8266)
    return ESP.getResetReason();
#elif defined(ESP32)
    switch (esp_reset_reason())
    {
    case ESP_RST_POWERON:
        return "Power-on reset";
    case ESP_RST_EXT:
        return "External reset";
    case ESP_RST_SW:
        return "Software reset";
    case ESP_RST_PANIC:
        return "Panic (hardware or software)";
    case ESP_RST_INT_WDT:
        return "Internal watchdog reset";
    case ESP_RST_TASK_WDT:
        return "Task watchdog reset";
    case ESP_RST_WDT:
        return "Watchdog reset";
    case ESP_RST_DEEPSLEEP:
        return "Deep sleep reset";
    case ESP_RST_BROWNOUT:
        return "Brownout reset";
    case ESP_RST_SDIO:
        return "SDIO reset";
    default:
        return "Unknown reset reason";
    }
#endif
}

Version parseVersion(const char *versionStr)
{
    Version version;

    memset(version.prerelease, 0, sizeof(version.prerelease));

    sscanf(versionStr, "%d.%d.%d-%s", &version.major, &version.minor, &version.patch, version.prerelease);
    return version;
}

int compareVersions(const char *version1, const char *version2)
{
    if (strlen(version1) == 0 || strlen(version2) == 0)
    {
        return 0;
    }

    Version v1 = parseVersion(version1);
    Version v2 = parseVersion(version2);

    if (v1.major != v2.major)
    {
        return v1.major - v2.major;
    }
    if (v1.minor != v2.minor)
    {
        return v1.minor - v2.minor;
    }
    if (v1.patch != v2.patch)
    {
        return v1.patch - v2.patch;
    }

    if (strlen(v1.prerelease) == 0 && strlen(v2.prerelease) == 0)
    {
        return 0; // Versions are equal
    }
    else if (strlen(v1.prerelease) == 0)
    {
        return 1; // v1 is greater (no prerelease for v1, but prerelease for v2)
    }
    else if (strlen(v2.prerelease) == 0)
    {
        return -1; // v2 is greater (no prerelease for v2, but prerelease for v1)
    }
    else
    {
        return strcmp(v1.prerelease, v2.prerelease); // Compare prerelease strings
    }
}

