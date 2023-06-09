#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "sunspec.h"

#define NUM_PANELS 2
#define CHART1_NUM_POINTS 300

/**
 * @brief Structure to pass all the lvgl objects to and from functions
*/
typedef struct
{
  SemaphoreHandle_t GuiLock;
  
  lv_obj_t *Panels[NUM_PANELS];

  // wifi/network etc. status
  lv_obj_t *lbl_Status;

  // SolarEdge hardware info
  lv_obj_t *lbl_C_Model;
  lv_obj_t *lbl_C_Version;
  lv_obj_t *lbl_C_SerialNumber;

  // SolarEdge counters
  lv_obj_t *lbl_I_AC_Energy_WH;
  lv_obj_t *lbl_I_AC_Power;
  lv_obj_t *lbl_I_AC_Frequency;
  lv_obj_t *lbl_I_Temp_Sink;

  // Chart with measurements;
  lv_obj_t *chart_Power;
  lv_chart_series_t *chart_Series;

  // gauges
  lv_obj_t *lbl_I_AC_CurrentA;
  lv_obj_t *lbl_I_AC_CurrentB;
  lv_obj_t *lbl_I_AC_CurrentC;

  lv_obj_t *lbl_I_AC_VoltageAB;
  lv_obj_t *lbl_I_AC_VoltageBC;
  lv_obj_t *lbl_I_AC_VoltageCA;
  
  lv_obj_t *arc_AmpA;
  lv_obj_t *arc_AmpB;
  lv_obj_t *arc_AmpC;

  lv_obj_t *arc_VoltA;
  lv_obj_t *arc_VoltB;
  lv_obj_t *arc_VoltC;
    
}GuiData_t;

/**
 * @brief The different modes for the status label
*/
typedef enum {
  GUI_STATE_WIFI_DISCONNECT,
  GUI_STATE_WIFI_CONNECTED,
  GUI_STATE_SE_DISCONNECTED,
  GUI_STATE_SE_CONNECTED,
  GUI_STATE_TIME
}TGuiState;


/**
 * @brief Configure the lvgl screen and panels
 * 
 * @param GuiData_t pointer to structure to receive all the lgvl objects
*/
esp_err_t GUI_Setup(GuiData_t *);


/**
  * @brief Update labels, charts and gauges
  *
  * @param GuiData_t pointer to structure containing the lvgl objects
  * @param SolarEdge_t  pointer to structure containing the translated modbus values
  */
esp_err_t GUI_UpdatePanels(GuiData_t *, SolarEdge_t*);


/**
  * @brief Change lbl_Status text during initialization. Once connected, show current time
  *
  * @param GuiData_t pointer to structure containing the lvgl objects
  * @param TGuiState enum for the current state
  */
esp_err_t GUI_SetStatus(GuiData_t *, TGuiState s);

/**
  * @brief Show the next panel on the lvgl-screen
  *
  * @param GuiData_t pointer to structure containing the lvgl objects
  */
esp_err_t GUI_TogglePanel(GuiData_t *gd);