#include <lvgl.h>
#include <TFT_eSPI.h>
#include "EEPROM.h"

#define LENGTH(x) (strlen(x) + 1)
#define EEPROM_SIZE 200

#define GPIO_PIN 22

static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); 

void lv_example_textarea_1(void);
static void btnm_event_handler(lv_event_t * e);

lv_obj_t *label;
lv_obj_t * ta;
lv_obj_t * btnm;

char pss[128] = "*11234";  
char passchange[] = "*0000#";  
void Status(const char* Label, lv_color_t color);
void open_door_gpio(lv_timer_t* timer);
void writeStringToFlash(const char* toStore, int startAddr);
void readStringFromFlash(int startAddr);
void reset_lable(lv_timer_t * timer);
bool passwordChangeMode = false;
bool newPasswordMode = false;
bool door_opened = false;         // Trạng thái cửa
lv_timer_t *door_timer = NULL;    // Biến lưu timer mở cửa
/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* Read the touchpad */
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

void setup() {
    Serial.begin(115200); 
    lv_init();
    EEPROM.begin(EEPROM_SIZE);
    pinMode(GPIO_PIN, OUTPUT);
    // Ensure password is initialized properly
    if (EEPROM.read(0) < 1 || EEPROM.read(0) > 1) {
        writeStringToFlash(pss, 1);
    }   
    readStringFromFlash(1); 
    Serial.println(pss);   
    tft.begin();          
    tft.setRotation(2);

    uint16_t calData[5] = {297, 3421, 437, 3430, 2};
    tft.setTouch(calData);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    lv_example_textarea_1();
    Serial.println("Setup done");
}

void loop() {
    lv_timer_handler();
    delay(5);
}


static void btnm_event_handler(lv_event_t *e) {
    lv_obj_t * obj = lv_event_get_target(e);
    ta = (lv_obj_t *) lv_event_get_user_data(e);
    const char *txt = lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj));

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_del_char(ta);
    }
    else if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0) {
        lv_event_send(ta, LV_EVENT_READY, NULL);

        if (strcmp(lv_textarea_get_text(ta), passchange) == 0 && !passwordChangeMode) {
            Status("Re-enter password", lv_color_hex(0xFFFF00));  
            passwordChangeMode = true; 
        }
        else if (passwordChangeMode && strcmp(lv_textarea_get_text(ta), pss) == 0) {
            Status("NEW PASSWORD", lv_color_hex(0xFFFF00)); 
            newPasswordMode = true;
        }
        else if (newPasswordMode) {
            strncpy(pss, lv_textarea_get_text(ta), sizeof(pss));  // Using strncpy for safety
            Serial.println(pss);
            writeStringToFlash(pss, 1);
            newPasswordMode = false;
            passwordChangeMode = false;
            lv_timer_create(reset_lable, 500, NULL);
        }
        else if (strcmp(lv_textarea_get_text(ta), pss) == 0) { 
            Serial.printf("Password correct\n");
            Status("OPEN DOOR", lv_color_hex(0x00FF00)); 
             if (!door_opened) {
                door_opened = true;
                if (door_timer == NULL) {
                    door_timer = lv_timer_create(open_door_gpio, 100, NULL);
                }
        }

        // Tạo timer reset giao diện sau 2 giây
        lv_timer_create([](lv_timer_t *reset_timer) {
            lv_timer_del(reset_timer); // Xóa timer reset
        }, 2000, NULL);


            lv_timer_create(reset_lable, 2000, NULL);  
            newPasswordMode = false;
            passwordChangeMode = false;
        }
        else {
            Status("Incorrect!", lv_color_hex(0xFF0000));  
            lv_timer_create(reset_lable, 2000, NULL);  
            newPasswordMode = false;
            passwordChangeMode = false;
        }
        lv_textarea_set_text(ta, "");
    } 
    else if (strcmp(txt, LV_SYMBOL_HOME) == 0) {
         lv_textarea_set_text(ta, "");
    }
    else {
        lv_textarea_add_text(ta, txt);
    }
}

void lv_example_textarea_1(void)
{
    ta = lv_textarea_create(lv_scr_act());
    label = lv_label_create(lv_scr_act()); 
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, "");
    lv_textarea_set_password_mode(ta, true);
    lv_obj_set_width(ta, lv_pct(60));
    lv_obj_set_pos(ta, 5, 30);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 27);


    Status("PASSWORD", lv_color_hex(0xADD8E6));

    lv_obj_add_state(ta, LV_STATE_FOCUSED);


    static const char * btnm_map[] = {
                                      
                                      LV_SYMBOL_NEW_LINE, LV_SYMBOL_HOME , LV_SYMBOL_BACKSPACE, "\n",
                                      "1", "2", "3", "\n",
                                      "4", "5", "6", "\n",
                                      "7", "8", "9", "\n",
                                     "*" , "0", "#", ""
                                     };

    btnm = lv_btnmatrix_create(lv_scr_act());
    lv_obj_set_size(btnm, 250, 240);
    lv_obj_set_style_border_color(btnm, lv_color_hex(0x808080), 0); 
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btnm, btnm_event_handler, LV_EVENT_VALUE_CHANGED, ta);
    lv_obj_clear_flag(btnm, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_btnmatrix_set_map(btnm, btnm_map);
}

void Status(const char* Label, lv_color_t color) {    
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_bg_color(ta, color, 0);
        lv_obj_set_style_border_color(ta, lv_color_hex(0x0000FF), 0); 
        lv_label_set_text(label, Label);
}

void writeStringToFlash(const char* toStore, int startAddr) {
    for (uint8_t i = 0; i < LENGTH(toStore); i++) {
        EEPROM.write(startAddr + i, toStore[i]);
    }
    EEPROM.commit();
}

void reset_lable(lv_timer_t * timer) {
    Status("PASSWORD", lv_color_hex(0xADD8E6)); 
    lv_timer_del(timer); 
}

void readStringFromFlash(int startAddr) {
    for (uint8_t i = 0; i < sizeof(pss); i++) {
        pss[i] = EEPROM.read(startAddr + i);
        if (pss[i] == '\0') break;
    }
}



void open_door_gpio(lv_timer_t *timer) {
    // Bật GPIO để mở cửa
    digitalWrite(GPIO_PIN, HIGH);
    Serial.println("Door Opened (GPIO ON)");

    // Tạo timer để tắt GPIO sau 2 giây
    lv_timer_t *close_timer = lv_timer_create([](lv_timer_t *close_timer) {
        digitalWrite(GPIO_PIN, LOW); // Tắt GPIO
        Serial.println("Door Closed (GPIO OFF)");
        door_opened = false;         // Cập nhật trạng thái
        lv_timer_del(close_timer);   // Xóa timer tắt cửa
    }, 2000, NULL);

    // Xóa timer mở cửa vì không cần nữa
    lv_timer_del(timer);
    door_timer = NULL; // Reset biến timer
}