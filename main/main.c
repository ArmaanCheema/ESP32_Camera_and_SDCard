#include <stdio.h>              //file opperations
#include "esp_log.h"            //for debugging and outputting log messagges
#include "esp_system.h"         //system level functions
#include "esp_err.h"            //error handling
#include "esp_camera.h"         //camera driver
#include "esp_vfs_fat.h"        //FAT support
#include "sdmmc_cmd.h"          //sdcard support (SDMMC control)
#include "driver/sdmmc_host.h"  //      ""
#include "driver/gpio.h"        //GPIO control
//#include "led_strip.h"          //for on board RGB LED
//#include "driver/rmt.h"         //for controls for the LED

//PIN SETUP
#define CAM_PWDN_PIN -1
#define CAM_RESET_PIN -1

#define CAM_PCLK_PIN 13
#define CAM_XCLK_PIN 15

#define CAM_SIOD_PIN 4
#define CAM_SIOC_PIN 5

#define CAM_VSYNC_PIN 6
#define CAM_HREF_PIN 7

#define CAM_Y9_PIN 16
#define CAM_Y8_PIN 17
#define CAM_Y7_PIN 18
#define CAM_Y6_PIN 12
#define CAM_Y5_PIN 10
#define CAM_Y4_PIN 8
#define CAM_Y3_PIN 9
#define CAM_Y2_PIN 11

#define SD_CMD_PIN 38
#define SD_CLK_PIN 39
#define SD_D0_PIN 40

#define LED_PIN 2
//#define RGB_LED_PIN 48

static int IMAGE_COUNT = 0;

#define TAG "CameraSDTest"

void init_camera(void){
    camera_config_t config = {
        //Setting up the pin connections
        .pin_reset = CAM_RESET_PIN,
        .pin_pwdn = CAM_PWDN_PIN,
        .pin_xclk = CAM_XCLK_PIN,
        .pin_sscb_sda = CAM_SIOD_PIN,
        .pin_sscb_scl = CAM_SIOC_PIN,
        .pin_d7 = CAM_Y9_PIN,
        .pin_d6 = CAM_Y8_PIN,
        .pin_d5 = CAM_Y7_PIN,
        .pin_d4 = CAM_Y6_PIN,
        .pin_d3 = CAM_Y5_PIN,
        .pin_d2 = CAM_Y4_PIN,
        .pin_d1 = CAM_Y3_PIN,
        .pin_d0 = CAM_Y2_PIN,
        .pin_vsync = CAM_VSYNC_PIN,
        .pin_href = CAM_HREF_PIN,
        .pin_pclk = CAM_PCLK_PIN,

        //Camera Settings
        .xclk_freq_hz = 20000000, //20 MHz
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_SVGA,// UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
        .jpeg_quality = 2, //0-63 lower number means higher quality
        .fb_count = 2,
    };

    //Check if inits
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE("Camera", "Camera init failed with error %s", esp_err_to_name(err));
    }

    //Check if actually captures the frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb || fb->len == 0) {
        ESP_LOGE(TAG, "Camera capture failed");
        return;
    }
}

void init_sdcard(void){
    //Setting up (explaining) the sd card system
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;  //Limited pins (only 3) -> limited system (so we use a 1 Bit system) 

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = SD_CLK_PIN;
    slot_config.cmd = SD_CMD_PIN;
    slot_config.d0 = SD_D0_PIN;

    //Setting up how it will interact with the sdcard
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 100 * 1024 * 1024 // 100 MB
    };

    //Keeps them pulled up (has to do with keeping a stable connection realted to noise, glitches, other failings)
    gpio_set_pull_mode(SD_CLK_PIN, GPIO_PULLUP_ONLY); //CLK
    gpio_set_pull_mode(SD_CMD_PIN, GPIO_PULLUP_ONLY); //CMD
    gpio_set_pull_mode(SD_D0_PIN, GPIO_PULLUP_ONLY); //D0

    //Check if mounted
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE("SD Card", "Failed to mount filesystem (%s) (0x%x)", esp_err_to_name(ret), ret);
        return;
    }

    sdmmc_card_print_info(stdout, card);
    ESP_LOGI("SD Card", "SD card mounted successfully");
}

void setup_extras(void){
    //Sensor Settings
    sensor_t* s = esp_camera_sensor_get();
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    /* Other Settings
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);           // 0 = disable , 1 = enable
    s->set_ae_level(s, 0);       // -2 to 2
    s->set_aec_value(s, 300);    // 0 to 1200
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable , 1 = enable
    s->set_wpc(s, 1);            // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
    s->set_lenc(s, 1);           // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
    s->set_vflip(s, 0);          // 0 = disable , 1 = enable
    s->set_dcw(s, 1);            // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
    */
    ESP_LOGI("Sensor", "Sensor Configured");

    //IO2 LED
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    ESP_LOGI("LED", "LED Configured");
}

void capture_and_save(void){
    //Indicate capture in progress
    gpio_set_level(LED_PIN, 1);

    //Grabs frame (using the way we defined above)
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb || fb->buf == 0) {
        ESP_LOGE("Camera", "Camera capture failed");
        return;
    }

    //Goes to location image will be stored in
    char filename[64];
    snprintf(filename, sizeof(filename), "/sdcard/image_%03d.jpg", IMAGE_COUNT);
    FILE *file = fopen(filename, "wb");
    if (!file) {
        ESP_LOGE("SD Card", "Failed to open file for writing");
        esp_camera_fb_return(fb);
        return;
    }

    //Takes frame and writes the image in the sdcard
    fwrite(fb->buf, 1, fb->len, file); //Writes the raw JPEG data buffer to the sdcard
    fclose(file);
    esp_camera_fb_return(fb);
    ESP_LOGI("SD Card", "Image saved to /sdcard/image_[COUNT].jpg");

    //Indicate capture finished
    gpio_set_level(LED_PIN, 0);
    ESP_LOGI("Main", "Full Capture Successful");

}

/* Pin 48 RGB LED
led_strip_handle_t init_led(void){
    led_strip_config_t led_config = {
        .strip_gpio_num = RGB_LED_PIN,
        .led_model = LED_MODEL_WS2812,
        .max_leds = 1,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 0,
        .flags.with_dma = 0
    };

    led_strip_handle_t led;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_config, &rmt_config, &led));
    ESP_LOGI("LED", "LED initialized sucessfully");
    return led;
}
*/

bool IO0_pressed(void){
    return gpio_get_level(GPIO_NUM_0) == 0; //When press IO0 is low, Low = 0
}

void app_main(void){
    init_camera();
    init_sdcard();
    setup_extras();
    ESP_LOGI("Main", "All initialized");

    //Give time for the whitebalance to work
    vTaskDelay(pdMS_TO_TICKS(1000));

    capture_and_save();
    ESP_LOGI("Main", "capture_and_save returned to Main");

    /*/Action On Press of BOOT/IO0 button
    bool wasPressed = false;
    while (true){
        bool isPressed = IO0_pressed();
        if (!isPressed && wasPressed){
            capture_and_save();
        }
        wasPressed = isPressed;
        vTaskDelay(pdMS_TO_TICKS(10));
    }*/
    

}