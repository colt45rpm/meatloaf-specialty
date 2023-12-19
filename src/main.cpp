#include <esp_system.h>
#include <nvs_flash.h>
#include <esp32/himem.h>
#include <driver/gpio.h>
#include <esp_console.h>
#include "linenoise/linenoise.h"

//#include "archive.h"

#include "../include/global_defines.h"
#include "../include/debug.h"

#include "device.h"
#include "keys.h"
#include "led.h"
#include "display.h"

//#include "disk-sounds.h"

#include "fnSystem.h"
#include "fnConfig.h"
#include "fnWiFi.h"

#include "fsFlash.h"
#include "fnFsSD.h"

/**************************/
// Meatloaf


#include "bus.h"
#include "ml_tests.h"

std::string statusMessage;
bool initFailed = false;


/**************************/


void main_shutdown_handler()
{
    Debug_println("Shutdown handler called");
    // Give devices an opportunity to clean up before rebooting

//    IEC.shutdown();
}

// Initial setup
void main_setup()
{
#ifdef DEBUG
    fnUartDebug.begin(DEBUG_SPEED);
    unsigned long startms = fnSystem.millis();
    
    Serial.print( ANSI_WHITE "\r\n\r\n" ANSI_BLUE_BACKGROUND "==============================" ANSI_RESET_NL );
    Serial.print( ANSI_BLUE_BACKGROUND "   " PRODUCT_ID " " FW_VERSION "   " ANSI_RESET_NL );
    Serial.print( ANSI_BLUE_BACKGROUND "   " PLATFORM_DETAILS "    " ANSI_RESET_NL );
    Serial.print( ANSI_BLUE_BACKGROUND "------------------------------" ANSI_RESET_NL "\r\n" );

    Serial.printf( "Meatloaf %s Started @ %lu\r\n", fnSystem.get_fujinet_version(), startms );

    Serial.printf( "Starting heap: %u\r\n", fnSystem.get_free_heap_size() );

#ifdef BOARD_HAS_PSRAM
    Serial.printf( "PsramSize %u\r\n", fnSystem.get_psram_size() );

    Serial.printf( "himem phys %u\r\n", esp_himem_get_phys_size() );
    Serial.printf( "himem free %u\r\n", esp_himem_get_free_size() );
    Serial.printf( "himem reserved %u\r\n", esp_himem_reserved_area_size() );
#endif


#endif // DEBUG

    // Install a reboot handler
    esp_register_shutdown_handler(main_shutdown_handler);

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        Serial.print("Erasing flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);

    // Enable GPIO Interrupt Service Routine
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    fnSystem.check_hardware_ver();
    Serial.printf("Detected Hardware Version: %s\r\n", fnSystem.get_hardware_ver_str());

    fnKeyManager.setup();
    fnLedManager.setup();

    // Enable/Disable Modem/Parallel Mode on Userport
    fnSystem.set_pin_mode(PIN_MODEM_ENABLE, gpio_mode_t::GPIO_MODE_OUTPUT);
    fnSystem.digital_write(PIN_MODEM_ENABLE, DIGI_LOW); // DISABLE Modem
    //fnSystem.digital_write(PIN_MODEM_ENABLE, DIGI_HIGH); // ENABLE Modem
    fnSystem.set_pin_mode(PIN_MODEM_UP9600, gpio_mode_t::GPIO_MODE_OUTPUT);
    fnSystem.digital_write(PIN_MODEM_UP9600, DIGI_LOW); // DISABLE UP9600
    //fnSystem.digital_write(PIN_MODEM_UP9600, DIGI_HIGH); // ENABLE UP9600

    fsFlash.start();
#ifdef SD_CARD
    fnSDFAT.start();
#endif

    // setup crypto key - must be done before loading the config
    crypto.setkey("MLK" + fnWiFi.get_mac_str());

    // Load our stored configuration
    Config.load();

    // Set up the WiFi adapter
    fnWiFi.start();

    // Setup IEC Bus
    IEC.setup();
    Serial.println( ANSI_GREEN_BOLD "IEC Bus Initialized" ANSI_RESET );

    {
        // Add devices to bus
        FileSystem *ptrfs = fnSDFAT.running() ? (FileSystem *)&fnSDFAT : (FileSystem *)&fsFlash;
        iecPrinter::printer_type ptype = iecPrinter::printer_type::PRINTER_COMMODORE_MPS803; // temporary
        Serial.printf("Creating a default printer using %s storage and type %d\r\n", ptrfs->typestring(), ptype);
        iecPrinter *ptr = new iecPrinter(ptrfs, ptype);
        fnPrinters.set_entry(0, ptr, ptype, Config.get_printer_port(0));
        Serial.print("Printer "); IEC.addDevice(ptr, 4); // add as device #4 for now

        Serial.print("Disk "); IEC.addDevice(new iecDrive(), 8);
        Serial.print("Network "); IEC.addDevice(new iecNetwork(), 16);
        Serial.print("CPM "); IEC.addDevice(new iecCpm(), 20);
        Serial.print("Voice "); IEC.addDevice(new iecVoice(), 21);
        Serial.print("Meatloaf "); IEC.addDevice(new iecMeatloaf(), 30);

        Serial.print("Virtual Device(s) Started: [ " ANSI_YELLOW_BOLD );
        for (uint8_t i = 0; i < 31; i++)
        {
            if (IEC.isDeviceEnabled(i))
            {
                Serial.printf("%.02d ", i);
            }
        }
        Serial.println( ANSI_RESET "]");
        //IEC.enabled = true;
    }

#ifdef PARALLEL_BUS
    // Setup Parallel Bus
    PARALLEL.setup();
    Serial.println( ANSI_GREEN_BOLD "Parallel Bus Initialized" ANSI_RESET );
#endif

#ifdef LED_STRIP
    // Start LED Strip
    display_app_main(); // fastled lib
#endif

#ifdef PIEZO_BUZZER
    mlSoundManager.setup(); // start sound
#endif

#ifdef DEBUG
    unsigned long endms = fnSystem.millis();
    Debug_printf("Available heap: %u\r\nSetup complete @ %lu (%lums)\r\n", fnSystem.get_free_heap_size(), endms, endms - startms);
#endif // DEBUG

#ifdef DEBUG_TIMING
    Debug_printv( ANSI_GREEN_BOLD "DEBUG_TIMING enabled" ANSI_RESET );
#endif

#ifdef RUN_TESTS
    runTestsSuite();
    // lfs_test();
#endif
}


// Main high-priority service loop
void fn_console_loop(void *param)
{
    esp_console_config_t  config = {
        .max_cmdline_length = 80,
        .max_cmdline_args = 10,
        .hint_color = 39
    };

    esp_err_t e = esp_console_init(&config);

    char* line;

    if(e == ESP_OK) {
        while((line = linenoise("hello> ")) != NULL) {
            printf("You wrote: %s\r\n", line);
            linenoiseFree(line); /* Or just free(line) if you use libc malloc. */
        }
    }

    esp_console_deinit();
}



/*
 * This is the start/entry point for an ESP-IDF program (must use "C" linkage)
 */
extern "C"
{
    void app_main()
    {
        // Call our setup routine
        main_setup();

        // Delete app_main() task since we no longer need it
        vTaskDelete(NULL);
    }
}
