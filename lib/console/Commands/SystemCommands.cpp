#include "SystemCommands.h"

#include <cstring>

#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <getopt.h>

#include <soc/efuse_reg.h>

#include <memory>
#include <soc/soc.h>
#include <esp_partition.h>

#include "soc/spi_reg.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_flash.h"

#include "../ESP32Console.h"

#include "Esp.h"

EspClass ESP;

static std::string mac2String(uint64_t mac)
{
    uint8_t *ar = (uint8_t *)&mac;
    std::string s;
    for (uint8_t i = 0; i < 6; ++i)
    {
        char buf[3];
        sprintf(buf, "%02X", ar[i]); // J-M-L: slight modification, added the 0 in the format for padding
        s += buf;
        if (i < 5)
            s += ':';
    }
    return s;
}

static const char *getFlashModeStr()
{
#if CONFIG_IDF_TARGET_ESP32S2
    const uint32_t spi_ctrl = REG_READ(PERIPHS_SPI_FLASH_CTRL);
#else
    const uint32_t spi_ctrl = REG_READ(SPI_CTRL_REG(0));
#endif
    /* Not all of the following constants are already defined in older versions of spi_reg.h, so do it manually for now*/
    if (spi_ctrl & BIT(24)) { //SPI_FREAD_QIO
        return "QIO";
    } else if (spi_ctrl & BIT(20)) { //SPI_FREAD_QUAD
        return "QOUT";
    } else if (spi_ctrl &  BIT(23)) { //SPI_FREAD_DIO
        return "DIO";
    } else if (spi_ctrl & BIT(14)) { // SPI_FREAD_DUAL
        return "DOUT";
    } else if (spi_ctrl & BIT(13)) { //SPI_FASTRD_MODE
        return "FAST READ";
    } else {
        return "SLOW READ";
    }
    return "DOUT";
}

static const char *getResetReasonStr()
{
    switch (esp_reset_reason())
    {
    case ESP_RST_BROWNOUT:
        return "Brownout reset (software or hardware)";
    case ESP_RST_DEEPSLEEP:
        return "Reset after exiting deep sleep mode";
    case ESP_RST_EXT:
        return "Reset by external pin (not applicable for ESP32)";
    case ESP_RST_INT_WDT:
        return "Reset (software or hardware) due to interrupt watchdog";
    case ESP_RST_PANIC:
        return "Software reset due to exception/panic";
    case ESP_RST_POWERON:
        return "Reset due to power-on event";
    case ESP_RST_SDIO:
        return "Reset over SDIO";
    case ESP_RST_SW:
        return "Software reset via esp_restart";
    case ESP_RST_TASK_WDT:
        return "Reset due to task watchdog";
    case ESP_RST_WDT:
        return "ESP_RST_WDT";

    case ESP_RST_UNKNOWN:
    default:
        return "Unknown";
    }
}

static int sysInfo(int argc, char **argv)
{
    esp_chip_info_t info;
    esp_chip_info(&info);

    printf("ESP32Console version: %s\n", ESP32CONSOLE_VERSION);
//    printf("Arduino Core version: %s (%x)\n", XTSTR(ARDUINO_ESP32_GIT_DESC), ARDUINO_ESP32_GIT_VER);
    printf("ESP-IDF Version: %s\n", ESP.getSdkVersion());

    printf("\n");
    printf("Chip info:\n");
    printf("\tModel: %s\n", ESP.getChipModel());
    printf("\tRevison number: %d\n", ESP.getChipRevision());
    printf("\tCores: %d\n", ESP.getChipCores());
    printf("\tClock: %lu MHz\n", ESP.getCpuFreqMHz());
    printf("\tFeatures:%s%s%s%s%s\r\n",
           info.features & CHIP_FEATURE_WIFI_BGN ? " 802.11bgn " : "",
           info.features & CHIP_FEATURE_BLE ? " BLE " : "",
           info.features & CHIP_FEATURE_BT ? " BT " : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? " Embedded-Flash " : " External-Flash ",
           info.features & CHIP_FEATURE_EMB_PSRAM ? " Embedded-PSRAM" : "");

    printf("EFuse MAC: %s\n", mac2String(ESP.getEfuseMac()).c_str());

    printf("Flash size: %ld MB (mode: %s, speed: %ld MHz)\n", ESP.getFlashChipSize() / (1024 * 1024), getFlashModeStr(), ESP.getFlashChipSpeed() / (1024 * 1024));
    printf("PSRAM size: %ld MB\n", ESP.getPsramSize() / (1024 * 1024));

#ifndef CONFIG_APP_REPRODUCIBLE_BUILD
    printf("Compilation datetime: " __DATE__ " " __TIME__ "\n");
#endif

    printf("\nReset reason: %s\n", getResetReasonStr());

    printf("\n");
    printf("CPU temperature: %.01f °C\n", ESP.temperatureRead());

    return EXIT_SUCCESS;
}

static int restart(int argc, char **argv)
{
    printf("Restarting...");
    ESP.restart();
    return EXIT_SUCCESS;
}

static int meminfo(int argc, char **argv)
{
    uint32_t free = ESP.getFreeHeap() / 1024;
    uint32_t total = ESP.getHeapSize() / 1024;
    uint32_t used = total - free;
    uint32_t min = ESP.getMinFreeHeap() / 1024;

    printf("Internal Heap: %lu KB free, %lu KB used, (%lu KB total)\n", free, used, total);
    printf("Minimum free heap size during uptime was: %lu KB\n", min);
    return EXIT_SUCCESS;
}

static int date(int argc, char **argv)
{
    bool set_time = false;
    char *target = nullptr;

    int c;
    opterr = 0;

    // Set timezone from env variable
    tzset();

    while ((c = getopt(argc, argv, "s")) != -1)
        switch (c)
        {
        case 's':
            set_time = true;
            break;
        case '?':
            printf("Unknown option: %c\n", optopt);
            return 1;
        case ':':
            printf("Missing arg for %c\n", optopt);
            return 1;
        }

    if (optind < argc)
    {
        target = argv[optind];
    }

    if (set_time)
    {
        if (!target)
        {
            fprintf(stderr, "Set option requires an datetime as argument in format '%%Y-%%m-%%d %%H:%%M:%%S' (e.g. 'date -s \"2022-07-13 22:47:00\"'\n");
            return 1;
        }

        tm t;

        if (!strptime(target, "%Y-%m-%d %H:%M:%S", &t))
        {
            fprintf(stderr, "Set option requires an datetime as argument in format '%%Y-%%m-%%d %%H:%%M:%%S' (e.g. 'date -s \"2022-07-13 22:47:00\"'\n");
            return 1;
        }

        timeval tv = {
            .tv_sec = mktime(&t),
            .tv_usec = 0};

        if (settimeofday(&tv, nullptr))
        {
            fprintf(stderr, "Could not set system time: %s", strerror(errno));
            return 1;
        }

        time_t tmp = time(nullptr);

        constexpr int buffer_size = 100;
        char buffer[buffer_size];
        strftime(buffer, buffer_size, "%a %b %e %H:%M:%S %Z %Y", localtime(&tmp));
        printf("Time set: %s\n", buffer);

        return 0;
    }

    // If no target was supplied put a default one (similar to coreutils date)
    if (!target)
    {
        target = (char*) "+%a %b %e %H:%M:%S %Z %Y";
    }

    // Ensure the format string is correct
    if (target[0] != '+')
    {
        fprintf(stderr, "Format string must start with an +!\n");
        return 1;
    }

    // Ignore + by moving pointer one step forward
    target++;

    constexpr int buffer_size = 100;
    char buffer[buffer_size];
    time_t t = time(nullptr);
    strftime(buffer, buffer_size, target, localtime(&t));
    printf("%s\n", buffer);
    return 0;

    return EXIT_SUCCESS;
}

namespace ESP32Console::Commands
{
    const ConsoleCommand getRestartCommand()
    {
        return ConsoleCommand("restart", &restart, "Restart / Reboot the system");
    }

    const ConsoleCommand getSysInfoCommand()
    {
        return ConsoleCommand("sysinfo", &sysInfo, "Shows informations about the system like chip model and ESP-IDF version");
    }

    const ConsoleCommand getMemInfoCommand()
    {
        return ConsoleCommand("meminfo", &meminfo, "Shows information about heap usage");
    }

    const ConsoleCommand getDateCommand()
    {
        return ConsoleCommand("date", &date, "Shows and modify the system time");
    }
}