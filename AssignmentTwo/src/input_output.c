#include <driver/gpio.h>

#include <esp_system.h>
#include "input_output.h"
#include <esp_system.h>
#include "fonts.h"
#include "graphics.h"
#include "demos.h"
#include <driver/touch_pad.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>


const int TOUCH_PADS[4]={2,3,9,8};

// for button inputs
QueueHandle_t inputQueue;
xTimerHandle repeatTimer;
uint64_t lastkeytime=0;
int keyrepeat=1;

static int button_val[2]={1,1};
static void repeatTimerCallback(xTimerHandle pxTimer) {
    int v;
    if(button_val[0]==0) {
        v=0;
        xQueueSendFromISR(inputQueue,&v,0);
    }
    if(button_val[1]==0) {
        v=35;
        xQueueSendFromISR(inputQueue,&v,0);
    }
    xTimerChangePeriod( repeatTimer, pdMS_TO_TICKS(200), 0);
    xTimerStart( repeatTimer, 0 );

}
// interrupt handler for button presses on GPIO0 and GPIO35
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    int gpio_index=(gpio_num==35);
    int val=(1-button_val[gpio_index]);

    uint64_t time=esp_timer_get_time();
    uint64_t timesince=time-lastkeytime;
    // ets_printf("gpio_isr_handler %d %d %lld\n",gpio_num,val, timesince);
    // the buttons can be very bouncy so debounce by checking that it's been .5ms since the last
    // change
    if(timesince>500) {
        int v=gpio_num+val*100;
        xQueueSendFromISR(inputQueue,&v,0);
        if(val==0 && keyrepeat) {
            xTimerChangePeriod( repeatTimer, pdMS_TO_TICKS(400), 0);
            xTimerStart( repeatTimer, 0 );
        }
        if(val==1 && keyrepeat) {
            xTimerStop( repeatTimer, 0 );
        }
        lastkeytime=time;
    }
    button_val[gpio_index]=val;
    
    gpio_set_intr_type(gpio_num,val==0?GPIO_INTR_HIGH_LEVEL:GPIO_INTR_LOW_LEVEL);

}

// get a button press, returns a key_type value which can be
// NO_KEY if no buttons have been pressed since the last call.
key_type get_input() {
    int key;
    if(xQueueReceive(inputQueue,&key,0)==pdFALSE)
        return NO_KEY;
    switch(key) {
        case 0: return LEFT_DOWN;
        case 35: return RIGHT_DOWN;
        case 100: return LEFT_UP;
        case 135: return RIGHT_UP;
    }
    return NO_KEY;
}

int demo_menu(char * title, int nentries, char *entries[], int select) {
    // for fps calculation

    int64_t current_time;
    int64_t last_time = esp_timer_get_time();
 
    int frame=0;
    while(1) {
        cls(0);
        setFont(FONT_DEJAVU18);

        draw_rectangle(0,3,display_width,24,rgbToColour(255,128,0));//banner
        draw_rectangle(0,select*18+24+3,display_width,18,rgbToColour(0,245,245));//highlight

        setFontColour(0, 0, 0);
        print_xy(title, 10, 8);
        setFontColour(255, 255, 255);
        setFont(FONT_UBUNTU16);
        for(int i=0;i<nentries;i++) {
            print_xy(entries[i],10,LASTY+((i==0)?21:18));
        }
        if(get_orientation()) {
            print_xy("\x86",4,display_height-16); // down arrow 
            print_xy("\x90",display_width-16,display_height-16); // OK
        }
        else {
            setFontColour(0, 0, 0);
            print_xy("\x90",display_width-16,4); // OK
            setFontColour(255, 255, 255);
            print_xy("\x86",display_width-16,display_height-16); // down arrow 
        }
        for (int i = 0; i <4; i++) {
            uint16_t touch_value;
            touch_pad_read(TOUCH_PADS[i], &touch_value);
            if(touch_value<1000) {
                int x=(i%2*120);
                int y=i>1?0:130;
                if(get_orientation())  
                    draw_rectangle(130-y,x,5,120,rgbToColour(200,200,255));
                else 
                    draw_rectangle(x,y,120,5,rgbToColour(200,200,255)); 
            }
        }
        flip_frame();
        current_time = esp_timer_get_time();
        if ((frame++ % 10) == 0) {
            printf("FPS:%f %d %d\n", 1.0e6 / (current_time - last_time),
                heap_caps_get_free_size(MALLOC_CAP_DMA),
                heap_caps_get_free_size(MALLOC_CAP_32BIT));
            vTaskDelay(1);
        }
        last_time = current_time;
        key_type key=get_input();
        if(key==LEFT_DOWN) select=(select+1)%nentries;
        if(key==RIGHT_DOWN) return select;
    }
}

void input_output_init() {
     // queue for button presses
    inputQueue = xQueueCreate(4,4);
    repeatTimer = xTimerCreate("repeat", pdMS_TO_TICKS(300),pdFALSE,(void*)0, repeatTimerCallback);
        // interrupts for button presses
    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_direction(35, GPIO_MODE_INPUT);
    gpio_set_intr_type(0, GPIO_INTR_LOW_LEVEL);
    gpio_set_intr_type(35, GPIO_INTR_LOW_LEVEL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(0, gpio_isr_handler, (void*) 0);
    gpio_isr_handler_add(35, gpio_isr_handler, (void*) 35);
    touch_pad_init();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for (int i = 0;i< 4;i++) {
        touch_pad_config(TOUCH_PADS[i], 0);
    }
}

uint16_t touch_values[4];
uint64_t touch_time=0;
uint64_t delay=400000;

const int ROWS=4;
const int COLS=12;
const int DEL_KEY = 0x7f;
const int SHIFT_KEY = 0x80;
const int ENTER_KEY = 0x81;
const char QWERTY_KEYS[2][48] = {"1234567890-=qwertyuiop[]asdfghjkl;'/\x80zxcvbnm,.\x7f\x81",
                        "!@#$%^&*()_+QWERTYUIOP{}ASDFGHJKL:\"?\x80ZXCVBNM<>\x7f\x81"};

void draw_keyboard(int topy, int highlight, int alt) {
    char str[2] = {0};
    draw_rectangle(0, topy, display_width, display_height - topy,
                   rgbToColour(32, 32, 42));
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int chi = r * COLS + c;
            int y = topy + (r * (display_height - topy)) / ROWS;
            int x = (c * display_width) / COLS;
            str[0] = QWERTY_KEYS[alt][chi];
            if (chi == highlight) {
                draw_rectangle(x, y, display_width / COLS,
                               (display_height - topy) / ROWS,
                               rgbToColour(120, 20, 20));
            }
            print_xy(str, x + 4, y + 4);
        }
}

void draw_controls(char *string, int sel) {
    setFontColour(0,0,0);
    char ch[2]={0};
    for(int i=0;i<strlen(string); i++) {
        int x=display_width-16*strlen(string)+i*16;
        ch[0]=string[i];
        if(i==sel)
            draw_rectangle(x,2,16,16,rgbToColour(255,255,255));
        print_xy(ch,x,2);
    }
}

nvs_handle_t storage_open(nvs_open_mode_t mode) {
    esp_err_t err;
    nvs_handle_t my_handle;
    err = nvs_open("storage", mode, &my_handle);
    if(err!=0) {
        nvs_flash_init();
        err = nvs_open("storage", mode, &my_handle);
        printf("err1: %d\n",err);
    }
    return my_handle;
}

int storage_read_int(char *name, int def) {
    nvs_handle_t handle=storage_open(NVS_READONLY);
    int val=def;
    nvs_get_i32(handle, name, &val);
    nvs_close(handle);
    return val;
}

void storage_write_int(char *name, int val) {
    nvs_handle_t handle=storage_open(NVS_READWRITE);
    nvs_set_i32(handle, name, val);
    nvs_commit(handle);
    nvs_close(handle);
}


void storage_read_string(char *name, char *def, char *dest, int len) {
    nvs_handle_t handle=storage_open(NVS_READONLY);
    strncpy(dest,def,len);
    size_t length=len;
    nvs_get_str(handle, name, dest, &length);
    nvs_close(handle);
    printf("Read %s = %s\n",name,dest);
}

void storage_write_string(char *name, char *val) {
    nvs_handle_t handle=storage_open(NVS_READWRITE);
    nvs_set_str(handle, name, val);
    nvs_commit(handle);
    nvs_close(handle);
     printf("Wrote %s = %s\n",name,val);
}

void edit_stored_string(char *name, char *prompt) {
    char val[64];
    storage_read_string(name,"",val,sizeof(val));
    get_string(prompt,val,sizeof(val));
    storage_write_string(name,val);
}