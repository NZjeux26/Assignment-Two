
#include <driver/gpio.h>

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>

#include "fonts.h"
#include "graphics.h"
#include "demos.h"
#include "input_output.h"

#define PAD_START 3
#define PAD_END 5

#define SHOW_PADS
/*
 This code has been modified from the espressif spi_master demo code
 it displays some demo graphics on the 240x135 LCD on a TTGO T-Display board.
*/

int is_emulator=0;

void app_main() {

    input_output_init();
   
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    // ===== Set time zone to NZST======
    setenv("TZ", "	NZST-12", 0);
    tzset();
    uint32_t user2=*((uint32_t *)0x3FF64024);
    if(user2!=0x70000000) is_emulator=1; 
    // ==========================
    time(&time_now);
    tm_info = localtime(&time_now);


    graphics_init();
    cls(0);
    int sel=0;
    while(1){//main menu
        cls(0);
        char *entries[]={"Play - Brick Game","Play - Goose Game","Play - Missle Brick","Highscores","Instructions", get_orientation()?"Landscape":"Portrait"};
        sel=demo_menu("Main Menu",sizeof(entries)/sizeof(char *),entries,sel);
        switch (sel)
        {
        case 0:
        brickgame();
        break;
        case 1:
        goosecheck(1);
        break;
        case 2:
        goosecheck(2);
        break;
        case 3:
        highscore_display();
        break;
        case 4:
        instructions_display();
        break;
        case 5:
        set_orientation(1-get_orientation());
        break;
        }
    }
}
