#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include <time.h>

#include "gui.h"

#define TAG "gui"


LV_IMG_DECLARE(se_logo); // se_logo.c, source: resource/se_logo.png, converted with https://lvgl.io/tools/imageconverter

#define VER_OFFSET_TOTAL_POWER 300
#define PANEL_HEIGHT 400

extern bool NTPTimeSynced; // set in main by the ntp callback handler.

void WattToUnits(char *buf, double watts)
{
#define NUM_UNITS 5
const char* units[NUM_UNITS] = {"W", "kW", "MW", "GW", "TW"};
int unitIndex = 0;

  while (watts >= 1000 && unitIndex < (NUM_UNITS-1))
  {
    watts /= 1000;
    unitIndex++;
  }

  sprintf(buf, "%.2f %s", watts, units[unitIndex]);
}


void ui_event_Screen(lv_event_t * e)
{
lv_event_code_t event_code = lv_event_get_code(e);
GuiData_t *gd = (GuiData_t*) lv_event_get_user_data(e);

  if(event_code == LV_EVENT_CLICKED)
    GUI_TogglePanel(gd);
}

//
// Setup the lvgl screen and associated panels.
// To prevent issues with lv_task_handler() @file lcd.cpp, we use a semaphore to signal
// completion of the setup. 
//
// All panels are added to the Panels array, to make switching between the items easy.
//

// lvgl needs to fix the unnamed enum types for LV_PART_ and LV_STATE_ 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
esp_err_t GUI_Setup(GuiData_t *data)
{
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(screen, ui_event_Screen, LV_EVENT_ALL, data);

  // panels:
  data->Panels[0] = lv_obj_create(screen);
  data->Panels[1] = lv_obj_create(screen);
  data->Panels[2] = nullptr;
    
  lv_obj_set_width(data->Panels[0], 800);
  lv_obj_set_height(data->Panels[0], PANEL_HEIGHT);
  lv_obj_set_align(data->Panels[0], LV_ALIGN_BOTTOM_MID);
  lv_obj_clear_flag(data->Panels[0], LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(data->Panels[0], lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(data->Panels[0], 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_width(data->Panels[1], 800);
  lv_obj_set_height(data->Panels[1], PANEL_HEIGHT);
  lv_obj_set_align(data->Panels[1], LV_ALIGN_BOTTOM_MID);
  lv_obj_clear_flag(data->Panels[1], LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(data->Panels[1], lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(data->Panels[1], 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_disp_load_scr(screen);

  //
  // static content:
  //

  lv_obj_t *ui_logo_SE = lv_img_create(screen);
  lv_img_set_src(ui_logo_SE, &se_logo);
  lv_obj_set_align(ui_logo_SE, LV_ALIGN_TOP_MID);

  lv_obj_t *ui_Label_Version = lv_label_create(screen);
  lv_obj_set_style_text_font(ui_Label_Version, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(ui_Label_Version, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label_Version, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_Label_Version, LV_ALIGN_TOP_RIGHT);
  lv_label_set_text(ui_Label_Version, _PROJECT_VER_);

  //
  // dynamic content:
  //

  // SolarEdge inverter model
  data->lbl_C_Model =  lv_label_create(screen);
  lv_obj_set_style_text_font(data->lbl_C_Model, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_C_Model, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_C_Model, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_C_Model, LV_ALIGN_TOP_LEFT);
  lv_label_set_text(data->lbl_C_Model, "");

  // SolarEdge inverter version
  data->lbl_C_Version =  lv_label_create(screen);
  lv_obj_set_style_text_font(data->lbl_C_Version, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_C_Version, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_C_Version, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_C_Version, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(data->lbl_C_Version, 0, 14); 
  lv_label_set_text(data->lbl_C_Version, "");

  // SolarEdge inverter serial number
  data->lbl_C_SerialNumber =  lv_label_create(screen);
  lv_obj_set_style_text_font(data->lbl_C_SerialNumber, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_C_SerialNumber, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_C_SerialNumber, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_C_SerialNumber, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(data->lbl_C_SerialNumber, 0, 28); 
  lv_label_set_text(data->lbl_C_SerialNumber, "");

  // status string  
  data->lbl_Status =  lv_label_create(screen);
  lv_obj_set_style_text_font(data->lbl_Status, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_Status, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_Status, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_Status, LV_ALIGN_TOP_MID);
  lv_obj_set_pos(data->lbl_Status, 0, 48);  // logo has a height of 45 pixels
  lv_label_set_text(data->lbl_Status, "");
 

  //
  // on first panel:
  //

  // Total energy production:
  data->lbl_I_AC_Energy_WH = lv_label_create(data->Panels[0]);
  lv_obj_set_width(data->lbl_I_AC_Energy_WH, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_Energy_WH, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_Energy_WH, LV_ALIGN_BOTTOM_MID);
  lv_label_set_text(data->lbl_I_AC_Energy_WH, "");

  // Power:
  data->lbl_I_AC_Power = lv_label_create(data->Panels[0]);
  lv_obj_set_style_text_font(data->lbl_I_AC_Power, &lv_font_montserrat_32, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_Power, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_Power, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_Power, LV_ALIGN_TOP_MID);
  lv_obj_set_style_text_color(data->lbl_I_AC_Power, LV_COLOR_MAKE16(0xd2, 0xe3, 0x36), 0);
  lv_label_set_text(data->lbl_I_AC_Power, "");

  // frequency:
  data->lbl_I_AC_Frequency = lv_label_create(data->Panels[0]);
  lv_obj_set_style_text_font(data->lbl_I_AC_Frequency, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_Frequency, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_Frequency, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_Frequency, LV_ALIGN_BOTTOM_LEFT);
  lv_label_set_text(data->lbl_I_AC_Frequency, "Freq: ??.?? Hz");

  // temperature:
  data->lbl_I_Temp_Sink= lv_label_create(data->Panels[0]);
  lv_obj_set_style_text_font(data->lbl_I_Temp_Sink, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_Temp_Sink, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_Temp_Sink, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_Temp_Sink, LV_ALIGN_BOTTOM_RIGHT);
  lv_label_set_text(data->lbl_I_Temp_Sink, "Temp: ??.?? °C");

  // Chart with the last power measurements
  data->chart_Power = lv_chart_create(data->Panels[0]);
  lv_obj_set_width(data->chart_Power, 600);
  lv_obj_set_height(data->chart_Power, 240);
  lv_obj_set_align(data->chart_Power, LV_ALIGN_CENTER);
  lv_chart_set_type(data->chart_Power, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(data->chart_Power, CHART1_NUM_POINTS);
  data->chart_Series = lv_chart_add_series(data->chart_Power, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

  // TODO: calculate max Y from number of fases, voltage and amp ?
  lv_chart_set_range(data->chart_Power, LV_CHART_AXIS_PRIMARY_Y, 0, 6000);
  lv_chart_set_axis_tick(data->chart_Power, LV_CHART_AXIS_PRIMARY_X,  10, 0, 12, 1, false, 50);
  lv_chart_set_axis_tick(data->chart_Power, LV_CHART_AXIS_PRIMARY_Y, 5, 0, 5, 1, true, 50);

  //
  // Gauges on second panel:
  //

  // Amp #A
  data->arc_AmpA = lv_arc_create(data->Panels[1]);
  lv_obj_set_width(data->arc_AmpA, 150);
  lv_obj_set_height(data->arc_AmpA, 150);
  lv_obj_set_align(data->arc_AmpA, LV_ALIGN_BOTTOM_LEFT);
  lv_arc_set_range(data->arc_AmpA, 0, 16);
  lv_obj_set_style_bg_opa(data->arc_AmpA, 10, LV_STATE_DEFAULT);
  lv_obj_set_style_arc_width(data->arc_AmpA, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  data->lbl_I_AC_CurrentA = lv_label_create(data->arc_AmpA);
  lv_obj_set_style_text_font(data->lbl_I_AC_CurrentA, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_CurrentA, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_CurrentA, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_CurrentA, LV_ALIGN_CENTER);
  lv_label_set_text(data->lbl_I_AC_CurrentA, "??.?? Amp");


  // Amp #B
  data->arc_AmpB = lv_arc_create(data->Panels[1]);
  lv_obj_set_width(data->arc_AmpB, 150);
  lv_obj_set_height(data->arc_AmpB, 150);
  lv_obj_set_align(data->arc_AmpB, LV_ALIGN_BOTTOM_MID);
  lv_arc_set_range(data->arc_AmpB, 0, 16);
  lv_obj_set_style_bg_opa(data->arc_AmpB, 10, LV_STATE_DEFAULT);
  lv_obj_set_style_arc_width(data->arc_AmpB, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  data->lbl_I_AC_CurrentB = lv_label_create(data->arc_AmpB);
  lv_obj_set_style_text_font(data->lbl_I_AC_CurrentB, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_CurrentB, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_CurrentB, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_CurrentB, LV_ALIGN_CENTER);
  lv_label_set_text(data->lbl_I_AC_CurrentB, "??.?? Amp");


  // AMP #C
  data->arc_AmpC = lv_arc_create(data->Panels[1]);
  lv_obj_set_width(data->arc_AmpC, 150);
  lv_obj_set_height(data->arc_AmpC, 150);
  lv_obj_set_align(data->arc_AmpC, LV_ALIGN_BOTTOM_RIGHT);
  lv_arc_set_range(data->arc_AmpC, 0, 16);
  lv_obj_set_style_bg_opa(data->arc_AmpC, 10, LV_STATE_DEFAULT);
  lv_obj_set_style_arc_width(data->arc_AmpC, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  data->lbl_I_AC_CurrentC = lv_label_create(data->arc_AmpC);
  lv_obj_set_style_text_font(data->lbl_I_AC_CurrentC, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_CurrentC, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_CurrentC, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_CurrentC, LV_ALIGN_CENTER);
  lv_label_set_text(data->lbl_I_AC_CurrentC, "??.?? Amp");
  

  // VOLT #A
  data->arc_VoltA = lv_arc_create(data->Panels[1]);
  lv_obj_set_width(data->arc_VoltA, 150);
  lv_obj_set_height(data->arc_VoltA, 150);
  lv_obj_set_align(data->arc_VoltA, LV_ALIGN_BOTTOM_LEFT);
  lv_arc_set_range(data->arc_VoltA, 0, 255);
  lv_obj_set_style_bg_opa(data->arc_VoltA, 10, LV_STATE_DEFAULT);
  lv_obj_set_pos(data->arc_VoltA, 0, -160);
  lv_obj_set_style_arc_width(data->arc_VoltA, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  data->lbl_I_AC_VoltageAB = lv_label_create(data->arc_VoltA);
  lv_obj_set_style_text_font(data->lbl_I_AC_VoltageAB, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_VoltageAB, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_VoltageAB, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_VoltageAB, LV_ALIGN_CENTER);
  lv_label_set_text(data->lbl_I_AC_VoltageAB, "???.?? Volt");


  // VOLT #B
  data->arc_VoltB = lv_arc_create(data->Panels[1]);
  lv_obj_set_width(data->arc_VoltB, 150);
  lv_obj_set_height(data->arc_VoltB, 150);
  lv_obj_set_align(data->arc_VoltB, LV_ALIGN_BOTTOM_MID);
  lv_arc_set_range(data->arc_VoltB, 0, 255);
  lv_obj_set_style_bg_opa(data->arc_VoltB, 10, LV_STATE_DEFAULT);
  lv_obj_set_pos(data->arc_VoltB, 0, -160);
  lv_obj_set_style_arc_width(data->arc_VoltB, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  data->lbl_I_AC_VoltageBC = lv_label_create(data->arc_VoltB);
  lv_obj_set_style_text_font(data->lbl_I_AC_VoltageBC, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_VoltageBC, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_VoltageBC, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_VoltageBC, LV_ALIGN_CENTER);
  lv_label_set_text(data->lbl_I_AC_VoltageBC, "???.?? Volt");
  

  // VOLT #C
  data->arc_VoltC = lv_arc_create(data->Panels[1]);
  lv_obj_set_width(data->arc_VoltC, 150);
  lv_obj_set_height(data->arc_VoltC, 150);
  lv_obj_set_align(data->arc_VoltC, LV_ALIGN_BOTTOM_RIGHT);
  lv_arc_set_range(data->arc_VoltC, 0, 255);
  lv_obj_set_style_bg_opa(data->arc_VoltC, 10, LV_STATE_DEFAULT);
  lv_obj_set_pos(data->arc_VoltC, 0, -160);
  lv_obj_set_style_arc_width(data->arc_VoltC, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  data->lbl_I_AC_VoltageCA = lv_label_create(data->arc_VoltC);
  lv_obj_set_style_text_font(data->lbl_I_AC_VoltageCA, &lv_font_montserrat_14, LV_STATE_DEFAULT);
  lv_obj_set_width(data->lbl_I_AC_VoltageCA, LV_SIZE_CONTENT);
  lv_obj_set_height(data->lbl_I_AC_VoltageCA, LV_SIZE_CONTENT);
  lv_obj_set_align(data->lbl_I_AC_VoltageCA, LV_ALIGN_CENTER);
  lv_label_set_text(data->lbl_I_AC_VoltageCA, "???.?? Volt");
  

  // hide all panels but first:
  lv_obj_add_flag(data->Panels[1], LV_OBJ_FLAG_HIDDEN);

  // signal lv_task_handle() code we are good
  xSemaphoreGive(data->GuiLock);


  return ESP_OK;
}
#pragma GCC diagnostic pop

esp_err_t GUI_UpdatePanels(GuiData_t *gd, SolarEdge_t *se)
{
char buf[32];

  lv_label_set_text(gd->lbl_C_Model, (const char *)se->C_Model);
  lv_label_set_text(gd->lbl_C_Version, (const char *)se->C_Version);
  lv_label_set_text(gd->lbl_C_SerialNumber, (const char *)se->C_SerialNumber);

  sprintf(buf, "Total: ");
  WattToUnits(buf+strlen(buf), se->I_AC_Energy_WH);
  lv_label_set_text(gd->lbl_I_AC_Energy_WH, buf);

  sprintf(buf, "Power: ");
  WattToUnits(buf+strlen(buf), se->I_AC_Power);
  lv_label_set_text(gd->lbl_I_AC_Power, buf);

  sprintf(buf, "Freq: %.2f Hz", se->I_AC_Frequency);
  lv_label_set_text(gd->lbl_I_AC_Frequency, buf);

  sprintf(buf, "Temp: %.2f °C", se->I_Temp_Sink);
  lv_label_set_text(gd->lbl_I_Temp_Sink, buf);


  // Amperage gauges:
  sprintf(buf, "%2.2f Amp", se->I_AC_CurrentA);
  lv_label_set_text(gd->lbl_I_AC_CurrentA, buf);
  
  sprintf(buf, "%2.2f Amp", se->I_AC_CurrentB);
  lv_label_set_text(gd->lbl_I_AC_CurrentB, buf);
  
  sprintf(buf, "%2.2f Amp", se->I_AC_CurrentC);
  lv_label_set_text(gd->lbl_I_AC_CurrentC, buf);
  
  lv_arc_set_value(gd->arc_AmpA, se->I_AC_CurrentA);
  lv_arc_set_value(gd->arc_AmpB, se->I_AC_CurrentB);
  lv_arc_set_value(gd->arc_AmpC, se->I_AC_CurrentC);


  // Voltage gauges:
  sprintf(buf, "%3.2f Volt", se->I_AC_VoltageAN);
  lv_label_set_text(gd->lbl_I_AC_VoltageAB, buf);
  
  sprintf(buf, "%3.2f Volt", se->I_AC_VoltageBN);
  lv_label_set_text(gd->lbl_I_AC_VoltageBC, buf);
  
  sprintf(buf, "%3.2f Volt", se->I_AC_VoltageCN);
  lv_label_set_text(gd->lbl_I_AC_VoltageCA, buf);
  
  lv_arc_set_value(gd->arc_VoltA, se->I_AC_VoltageAN);
  lv_arc_set_value(gd->arc_VoltB, se->I_AC_VoltageBC);
  lv_arc_set_value(gd->arc_VoltC, se->I_AC_VoltageCN);

  // add latest measurement to the chart
  lv_chart_set_next_value(gd->chart_Power, gd->chart_Series, se->I_AC_Power);
  
  lv_chart_refresh(gd->chart_Power);
  
  return ESP_OK;
}

esp_err_t GUI_TogglePanel(GuiData_t *gd)
{
static uint8_t toggle;

  toggle++;
  if (toggle >= NUM_PANELS)
    toggle = 0;

  for (uint8_t i = 0; i != NUM_PANELS; i++)
    if (gd->Panels[i])
      lv_obj_add_flag(gd->Panels[i], LV_OBJ_FLAG_HIDDEN);

  lv_obj_clear_flag(gd->Panels[toggle], LV_OBJ_FLAG_HIDDEN);

  return ESP_OK;
}

esp_err_t GUI_SetStatus(GuiData_t *gd, TGuiState s)
{
  switch (s)
  {
    case GUI_STATE_WIFI_DISCONNECT:
      lv_label_set_text_static(gd->lbl_Status, "WiFi connecting");
    break;

    case GUI_STATE_WIFI_CONNECTED:
      lv_label_set_text_static(gd->lbl_Status, "WiFi connected");
    break;

    case GUI_STATE_SE_DISCONNECTED:
      lv_label_set_text_static(gd->lbl_Status, "Connecting to inverter");
    break;

    case GUI_STATE_SE_CONNECTED:
      lv_label_set_text_static(gd->lbl_Status, "-online-");
    break;

    case GUI_STATE_TIME:
      if (NTPTimeSynced)
      {
        static char buf[24];
        time_t t = time(NULL);
        struct tm *ltm = localtime(&t);

        strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", ltm);
        lv_label_set_text_static(gd->lbl_Status, buf);
      }
      else
        lv_label_set_text_static(gd->lbl_Status, "-waiting for clock sync-");
    break;
  }
  return ESP_OK;
}