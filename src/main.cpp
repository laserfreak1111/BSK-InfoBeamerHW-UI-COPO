#include <Arduino.h>
#include "pin_config.h"
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <ui.h>
#include "loading_texts.h"  
#include <OneButton.h>
#include <RotaryEncoder.h>
#include <ArduinoJson.h>
#define LV_BUTTON                _BV(0)
#define LV_ENCODER_CW            _BV(1)
#define LV_ENCODER_CCW           _BV(2)
#define ONBOARDLED 14

TFT_eSPI tft = TFT_eSPI(170, 320);
RotaryEncoder encoder(PIN_ENCODE_A, PIN_ENCODE_B, RotaryEncoder::LatchMode::FOUR3);
OneButton button(PIN_ENCODE_BTN, true);

static EventGroupHandle_t lv_input_event;
EventGroupHandle_t global_event_group;

lv_group_t *g;

static uint32_t last_update = 0;
static uint32_t last_update_fast = 0;

static uint32_t last_update_statusupdates = 0;
bool loading_done = true;


int mainmenu_pos_last = 0;
int dmxmenu_pos_last = 0;

bool skip_heartbeat = true;
bool heartbeat_lost = true;
bool ui_init_complete = false;

bool first_hearbeat = false;
bool firststart = true;
// DataValues

int val_dmx_safety_adr = 100;
int val_dmx_fire_adr = 101;
float val_co2_rate = 10;
float val_co2_saved = 0;

bool status_dmx_ok = false;
bool status_network_ok = false;

bool status_fire_on = false;
bool status_safety_on = false;
String status_network_ip = "0.0.0.0";
int status_cputemp = 1;
float status_co2usage = 0;
static lv_obj_t* last_screen = nullptr;


enum AppScreen {
    SCREEN_HOME,
    SCREEN_MAINMENU,
    SCREEN_DMX,
    SCREEN_INFO,
    SCREEN_Status,
    SCREEN_ValueSet_ADRSET_SAFETY,
    SCREEN_ValueSet_ADRSET_FIRE,
    SCREEN_Co2Menu,
     SCREEN_ValueSet_Co2Weight
};

AppScreen currentScreen = SCREEN_HOME;
AppScreen lastScreen = SCREEN_HOME;
typedef struct {
    uint8_t cmd;
    uint8_t data[14];
    uint8_t len;
} lcd_cmd_t;

lcd_cmd_t lcd_st7789v[] = {
    {0x11, {0}, 0 | 0x80},
    {0x3A, {0X05}, 1},
    {0xB2, {0X0B, 0X0B, 0X00, 0X33, 0X33}, 5},
    {0xB7, {0X75}, 1},
    {0xBB, {0X28}, 1},
    {0xC0, {0X2C}, 1},
    {0xC2, {0X01}, 1},
    {0xC3, {0X1F}, 1},
    {0xC6, {0X13}, 1},
    {0xD0, {0XA7}, 1},
    {0xD0, {0XA4, 0XA1}, 2},
    {0xD6, {0XA1}, 1},
    {0xE0, {0XF0, 0X05, 0X0A, 0X06, 0X06, 0X03, 0X2B, 0X32, 0X43, 0X36, 0X11, 0X10, 0X2B, 0X32}, 14},
    {0xE1, {0XF0, 0X08, 0X0C, 0X0B, 0X09, 0X24, 0X2B, 0X22, 0X43, 0X38, 0X15, 0X16, 0X2F, 0X37}, 14},
};

static void lv_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h);
  

    lv_disp_flush_ready(disp);
}

static void lv_encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    EventBits_t bit = xEventGroupGetBits(lv_input_event);
    data->state = LV_INDEV_STATE_RELEASED;
    if (bit & LV_BUTTON) {
        xEventGroupClearBits(lv_input_event, LV_BUTTON);
        data->state = LV_INDEV_STATE_PR;
    } else if (bit & LV_ENCODER_CW) {
        xEventGroupClearBits(lv_input_event, LV_ENCODER_CW);
        data->enc_diff = 1;
    } else if (bit & LV_ENCODER_CCW) {
        xEventGroupClearBits(lv_input_event, LV_ENCODER_CCW);
        data->enc_diff = -1;
    }
}



void send_value_to_host(const char* key, float value) {
  StaticJsonDocument<256> doc;
  doc["type"] = "set";
  JsonObject payload = doc.createNestedObject("payload");
  payload[key] = value;

  serializeJson(doc, Serial);
  Serial.println();
}

void send_command_to_host(const char* key, float value) {
  StaticJsonDocument<256> doc;
  doc["type"] = "cmd";
  JsonObject payload = doc.createNestedObject("payload");
  payload[key] = value;

  serializeJson(doc, Serial);
  Serial.println();
}

#define MAX_ROLLER_STRING_LEN 512
static char roller_string[MAX_ROLLER_STRING_LEN] = "";
void roller_add_line(const char *line) {
    if (strlen(roller_string) + strlen(line) + 2 < MAX_ROLLER_STRING_LEN) {
        if (strlen(roller_string) > 0) {
            strcat(roller_string, "\n");  // Zeilenumbruch, falls nicht erster Eintrag
        }
        strcat(roller_string, line);      // Neuen Eintrag anhängen
    } else {
        // Optional: Fehlerbehandlung
        printf("Roller-String zu lang!\n");
    }
}

const char* roller_get_string(void) {
    return roller_string;
}

void roller_clear(lv_obj_t *roller) {
    roller_string[0] = '\0';  // internen String zurücksetzen
    lv_roller_set_options(roller, "", LV_ROLLER_MODE_NORMAL);  // Roller anzeigen, aber leer
}

void drawinfoscreen(){

roller_clear(ui_rollerinfo);
roller_add_line("SERIAL: BSKWB000001");
roller_add_line("Core-SW 1.0.1");
roller_add_line("Core-HW BSK00150-1");
roller_add_line("< Exit");


    lv_roller_set_options(ui_rollerinfo ,roller_get_string(), LV_ROLLER_MODE_NORMAL);
}




void drawsatusscren(){
roller_clear(ui_rollerinfo);

char ip_line[64];
snprintf(ip_line, sizeof(ip_line), "IP: %s", status_network_ip);
roller_add_line(ip_line);

// Dynamischer Eintrag: Temperatur
float temp = status_cputemp;
char line[64];
snprintf(line, sizeof(line), "CPU-TEMP: %.1f °C", temp);
roller_add_line(line);
roller_add_line("SYSTEMLOAD: LOW");

roller_add_line("< Exit");

 lv_roller_set_options(ui_rollerinfo ,roller_get_string(), LV_ROLLER_MODE_NORMAL);
}



void updateco2usage()
{
    char buf[16];  // Genug Puffer für Zahl + Nullterminator
  /*Use the user_data*/
  snprintf(buf, sizeof(buf), "%.0f kg", status_co2usage);

  lv_label_set_text(ui_lbco2saved,buf);


 

 
}

void updatehomescreenadr(){



        char buffer[10]; // Puffer für den Text
        snprintf(buffer, sizeof(buffer), "%d", val_dmx_fire_adr);
        lv_label_set_text(ui_lbfireadr, buffer);
        snprintf(buffer, sizeof(buffer), "%d", val_dmx_safety_adr);
        lv_label_set_text(ui_lbsafeadr, buffer); 

}

void screenchanged() {


    lv_group_remove_all_objs(g);
    lv_group_focus_obj(NULL); // Fokus komplett entfernen+

    if (currentScreen == SCREEN_HOME) {
       // lv_group_add_obj(g, ui_Slider1);
        //lv_group_add_obj(g, ui_Slider2);
        //lv_group_add_obj(g, ui_Button2);
        updatehomescreenadr();

           mainmenu_pos_last = 0;
    }
    if (currentScreen == SCREEN_MAINMENU) {
        lv_group_add_obj(g, ui_rollermainmenu);
        lv_group_focus_obj(ui_rollermainmenu);  // Fokus
        lv_group_set_editing(g, true);
        lv_roller_set_selected(ui_rollermainmenu,mainmenu_pos_last,LV_ANIM_OFF);
    }
    if (currentScreen == SCREEN_DMX) {
        lv_group_add_obj(g, ui_rollerdmxmenu);
             lv_group_focus_obj(ui_rollerdmxmenu);      // << NEU
               lv_group_set_editing(g,true);
                lv_roller_set_selected(ui_rollermainmenu,dmxmenu_pos_last,LV_ANIM_OFF);
    }


        if (currentScreen == SCREEN_Co2Menu) {
        lv_group_add_obj(g, ui_rollerco2settings);
             lv_group_focus_obj(ui_rollerco2settings);      // << NEU
               lv_group_set_editing(g,true);
                
    }

      if (currentScreen == SCREEN_INFO) {
         drawinfoscreen();
        lv_group_add_obj(g, ui_rollerinfo);
             lv_group_focus_obj(ui_rollerinfo);      // << NEU
               lv_group_set_editing(g,true);
    }
    
     if (currentScreen == SCREEN_Status) {
        drawsatusscren();
        lv_group_add_obj(g, ui_rollerinfo);
             lv_group_focus_obj(ui_rollerinfo);      // << NEU
               lv_group_set_editing(g,true);
    }
    

    if (currentScreen == SCREEN_ValueSet_ADRSET_SAFETY) {

        lv_label_set_text(ui_lbadrsetheader1,"DMX Safety Channel:");
        
        lv_spinbox_set_digit_format(ui_Spinbox1,3,0);
        lv_spinbox_set_range(ui_Spinbox1,1,512);
        lv_group_add_obj(g, ui_Spinbox1);
        lv_group_focus_obj(ui_Spinbox1);      // << NEU
        lv_spinbox_set_value(ui_Spinbox1,val_dmx_safety_adr);
        lv_group_set_editing(g,true);
    }


    if (currentScreen == SCREEN_ValueSet_ADRSET_FIRE ){
        lv_label_set_text(ui_lbadrsetheader1,"DMX Fire Channel:");
        lv_spinbox_set_digit_format(ui_Spinbox1,3,0);
        lv_spinbox_set_range(ui_Spinbox1,1,512);
        lv_group_add_obj(g, ui_Spinbox1);
        lv_group_focus_obj(ui_Spinbox1);      // << NEU
        lv_spinbox_set_value(ui_Spinbox1,val_dmx_fire_adr);
        lv_group_set_editing(g,true);
    }


    if(currentScreen == SCREEN_ValueSet_Co2Weight){
    lv_label_set_text(ui_lbadrsetheader1,"Set Co2 Rate kg/s:");
   // lv_group_add_obj(g, ui_btnsetvalueback);
    lv_spinbox_set_digit_format(ui_Spinbox1,4,3);
    lv_spinbox_set_range(ui_Spinbox1,0,500);
    lv_group_add_obj(g, ui_Spinbox1);
    lv_group_focus_obj(ui_Spinbox1);      // << NEU
    lv_spinbox_set_value(ui_Spinbox1,val_co2_rate*10);
    lv_group_set_editing(g,true);
    }

}


void switchToScreen(AppScreen newScreen, lv_obj_t* newScreenObj) {
    lv_scr_load(newScreenObj);
    lastScreen = currentScreen;
    currentScreen = newScreen;
      screenchanged();

    

}



void handleHomeClick() {
    switchToScreen(SCREEN_MAINMENU, ui_mainmenu);
}

void handleMainMenuClick() {
    int selected = lv_roller_get_selected(ui_rollermainmenu);
    mainmenu_pos_last = selected;
    if (selected == 0) switchToScreen(SCREEN_DMX, ui_dmxmenu);
     if (selected == 2) switchToScreen(SCREEN_Co2Menu, ui_co2menu);
     if (selected == 3) switchToScreen(SCREEN_INFO, ui_info);
      if (selected == 4) switchToScreen(SCREEN_Status, ui_info);
    if (selected == 5) switchToScreen(SCREEN_HOME, ui_home);
}

void handleDmxMenuClick() {
    int selected = lv_roller_get_selected(ui_rollerdmxmenu);
        dmxmenu_pos_last = selected;
        if (selected == 0) switchToScreen(SCREEN_ValueSet_ADRSET_SAFETY, ui_valueset);
        if (selected == 1) switchToScreen(SCREEN_ValueSet_ADRSET_FIRE, ui_valueset);
        if (selected == 2) switchToScreen(SCREEN_MAINMENU, ui_mainmenu);

}


void handleco2MenuClick() {
    int selected = lv_roller_get_selected(ui_rollerco2settings);
       // dmxmenu_pos_last = selected;
        if (selected == 0) switchToScreen(SCREEN_ValueSet_Co2Weight, ui_valueset);
        //if (selected == 1) switchToScreen(SCREEN_MAINMENU, ui_mainmenu);
        if (selected == 2) switchToScreen(SCREEN_MAINMENU, ui_mainmenu);
}
void handleInfoClick() {
    int selected = lv_roller_get_selected(ui_rollerinfo);
    switchToScreen(SCREEN_MAINMENU, ui_mainmenu);
}

void handlestatusClick() {
    int selected = lv_roller_get_selected(ui_rollerinfo);
    switchToScreen(SCREEN_MAINMENU, ui_mainmenu);
}







void handlesetadrsafetyClick() {
  xEventGroupSetBits(lv_input_event, LV_BUTTON);
}

void handlesetadrfireClick() {
  xEventGroupSetBits(lv_input_event, LV_BUTTON);
}


void handle_valueset_co2weight() {

     xEventGroupSetBits(lv_input_event, LV_BUTTON);


}


void handleClick() {
    switch (currentScreen) {
        case SCREEN_HOME: handleHomeClick(); break;
        case SCREEN_MAINMENU: handleMainMenuClick(); break;
        case SCREEN_DMX: handleDmxMenuClick(); break;
        case SCREEN_INFO: handleInfoClick(); break;
         case SCREEN_Status: handlestatusClick(); break;
        case SCREEN_ValueSet_ADRSET_SAFETY: handlesetadrsafetyClick(); break;
        case SCREEN_ValueSet_ADRSET_FIRE: handlesetadrfireClick(); break;
        case SCREEN_Co2Menu: handleco2MenuClick(); break;
        case SCREEN_ValueSet_Co2Weight: handle_valueset_co2weight(); break;
        default: break;
    }
}


void handleLongPress() {

switch (currentScreen) {

        case SCREEN_ValueSet_Co2Weight: 
            val_co2_rate = lv_spinbox_get_value(ui_Spinbox1)/10.0;
            send_value_to_host("val_co2_rate", val_co2_rate);
            switchToScreen(SCREEN_Co2Menu, ui_co2menu);
            break;
        case SCREEN_Co2Menu: 
            
        // dmxmenu_pos_last = selected;
            if (lv_roller_get_selected(ui_rollerco2settings) == 1){
                  send_command_to_host("clear_usage", 0);
                  switchToScreen(SCREEN_MAINMENU, ui_mainmenu);
            }
            break;
        case SCREEN_ValueSet_ADRSET_FIRE: 
           val_dmx_fire_adr = lv_spinbox_get_value(ui_Spinbox1);
             send_value_to_host("val_dmx_fire_adr", val_dmx_fire_adr);
           switchToScreen(SCREEN_DMX, ui_dmxmenu);   
            break;
        case SCREEN_ValueSet_ADRSET_SAFETY: 
            val_dmx_safety_adr = lv_spinbox_get_value(ui_Spinbox1);
              send_value_to_host("val_dmx_safety_adr", val_dmx_safety_adr);
           switchToScreen(SCREEN_DMX, ui_dmxmenu);   
            break;
        default: break;
    }

}
void update_loading_label() {
    int idx;
    int attempts = 0;
    do {
        idx = rand() % NUM_LOADING_TEXTS;
        attempts++;
        if (attempts > 100) break;
    } while (is_recent(idx));

    add_to_history(idx);
    lv_label_set_text(uic_lbstart, loading_texts[idx]);
}

void setup() {
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf1, *buf2;

    global_event_group = xEventGroupCreate();
    lv_input_event = xEventGroupCreate();

    pinMode(ONBOARDLED, OUTPUT);
    digitalWrite(ONBOARDLED, LOW);
    Serial.begin(115200);
    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);


        //Set PWM Screen Brightness
      ledcSetup(0, 5000, 8);
      ledcAttachPin(15, 0);
      ledcWrite(0, 120);
 

    // Update Embed initialization parameters
    for (uint8_t i = 0; i < (sizeof(lcd_st7789v) / sizeof(lcd_cmd_t)); i++) {
        tft.writecommand(lcd_st7789v[i].cmd);
        for (int j = 0; j < (lcd_st7789v[i].len & 0x7f); j++) {
            tft.writedata(lcd_st7789v[i].data[j]);
        }

        if (lcd_st7789v[i].len & 0x80) {
            delay(120);
        }
    }



    //button.attachClick([](void *param) {
     //   xEventGroupSetBits((EventGroupHandle_t *)param, LV_BUTTON);
    //}, lv_input_event);


    button.attachClick(handleClick);
        button.attachLongPressStart(handleLongPress);

    lv_init();
    buf1 = (lv_color_t *)ps_malloc(LV_BUF_SIZE * sizeof(lv_color_t));
    buf2 = (lv_color_t *)ps_malloc(LV_BUF_SIZE * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LV_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_SCREEN_WIDTH;
    disp_drv.ver_res = LV_SCREEN_HEIGHT;
    disp_drv.flush_cb = lv_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = lv_encoder_read;
    indev_drv.user_data = lv_input_event;
    static lv_indev_t *lv_encoder_indev = lv_indev_drv_register(&indev_drv);

    ui_init();

    g = lv_group_create();
    lv_indev_set_group(lv_encoder_indev, g);

    attachInterrupt(digitalPinToInterrupt(PIN_ENCODE_A), []() {
        encoder.tick();
    }, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODE_B), []() {
        encoder.tick();
    }, CHANGE);
}

void handleEncoderInput() {
    RotaryEncoder::Direction dir = encoder.getDirection();
    if (dir != RotaryEncoder::Direction::NOROTATION) {
        xEventGroupSetBits(lv_input_event,
            dir == RotaryEncoder::Direction::CLOCKWISE ? LV_ENCODER_CCW : LV_ENCODER_CW);
    }
    button.tick();
}


void fasttimer(){

      uint32_t now = millis();
        if (now - last_update_fast >= 100) {
              updateco2usage();
            last_update_fast = now;
        }



  
}

void slowtimer(){

      uint32_t now = millis();
        if (now - last_update_statusupdates >=500) {

            if(status_dmx_ok){
        lv_img_set_src(ui_imgdmx, &ui_img_dmxicon_en_png);   
            }else{
         lv_img_set_src(ui_imgdmx, &ui_img_dmxicon_di_png);   
            }
 
      if(status_network_ok){
        lv_img_set_src(ui_imgeth, &ui_img_1667256483);   
            }else{
         lv_img_set_src(ui_imgeth, &ui_img_1748321345);   
            }

/*   if(heartbeat_lost){
        lv_img_set_src(ui_imgheart, &ui_img_1934214896);   
            }else{
         lv_img_set_src(ui_imgheart, &ui_img_467686951);   
            }
 */

            if(status_safety_on){
             lv_obj_set_style_bg_color(ui_consafe, lv_color_hex(0xFFD900), LV_PART_MAIN | LV_STATE_DEFAULT); //gelb
            }else{
            lv_obj_set_style_bg_color(ui_consafe, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT); //grau
            }

  if(status_fire_on){
             lv_obj_set_style_bg_color(ui_confire, lv_color_hex(0xC80000), LV_PART_MAIN | LV_STATE_DEFAULT); //rot
            }else{
            lv_obj_set_style_bg_color(ui_confire, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT); //grau
            }

            last_update_statusupdates = now;
        }


        
  
}






unsigned long last_heartbeat_ms = 0;


void process_json(String jsonStr) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) return;

  String type = doc["type"];

  if (type == "heartbeat") {
    last_heartbeat_ms = millis();
    heartbeat_lost = false;
   if(!first_hearbeat){
        if(ui_init_complete){
              switchToScreen(SCREEN_HOME, ui_home);
                first_hearbeat = true;
        } 
     
    }

  }
  else if (type == "status") {
    JsonObject payload = doc["payload"];
    if (payload.containsKey("status_dmx_ok")) status_dmx_ok = payload["status_dmx_ok"];
    if (payload.containsKey("status_network_ok")) status_network_ok = payload["status_network_ok"];
    if (payload.containsKey("status_fire_on")) status_fire_on = payload["status_fire_on"];
    if (payload.containsKey("status_safety_on")) status_safety_on = payload["status_safety_on"];
    if (payload.containsKey("status_cputemp")) status_cputemp = payload["status_cputemp"];
    if (payload.containsKey("status_co2usage")) status_co2usage = payload["status_co2usage"];
    if (payload.containsKey("status_network_ip")) status_network_ip = payload["status_network_ip"].as<String>();
  }
  else if (type == "get") {
    StaticJsonDocument<512> response;
    response["type"] = "ack";
    JsonObject payload = response.createNestedObject("payload");
    payload["val_dmx_safety_adr"] = val_dmx_safety_adr;
    payload["val_dmx_fire_adr"] = val_dmx_fire_adr;
    payload["val_co2_rate"] = val_co2_rate;
    payload["val_co2_saved"] = val_co2_saved;
    serializeJson(response, Serial);
    Serial.println();
  }else if (type == "set") {
  JsonObject payload = doc["payload"];
  if (payload.containsKey("val_dmx_safety_adr")) val_dmx_safety_adr = payload["val_dmx_safety_adr"];
  if (payload.containsKey("val_dmx_fire_adr")) val_dmx_fire_adr = payload["val_dmx_fire_adr"];
  if (payload.containsKey("val_co2_rate")) val_co2_rate = payload["val_co2_rate"];
  if (payload.containsKey("val_co2_saved")) val_co2_saved = payload["val_co2_saved"];
  updatehomescreenadr();
}
}







String inputBuffer;

void serial_loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      process_json(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  // Heartbeat überwachen
  if (millis() - last_heartbeat_ms > 2000) {
    heartbeat_lost = true;
  }
}


void loop() {
   


  serial_loop();


fasttimer();
slowtimer();


 if(millis()>6000){
    ui_init_complete=true;

}


/*     if (!loading_done) {
        uint32_t now = millis();
        if (now - last_update >= 3000) {
            update_loading_label();
            last_update = now;
        }
    } */

    handleEncoderInput();
    lv_timer_handler();
    
 delay(3);

              lv_obj_t* active = lv_scr_act();
        if (active != last_screen) {
            // Screen hat sich sichtbar geändert
            last_screen = active;
        }





}
