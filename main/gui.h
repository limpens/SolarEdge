#pragma once

#include <esp_err.h>

#include <lvgl.h>

#include "sunspec.h"

enum { PANEL_CHART_1H = 0, PANEL_CHART_24H, PANEL_GAUGE, PANEL_MAX };

#define CHART_24H_NUM_POINTS 240
#define CHART_1H_NUM_POINTS  360

typedef struct
{
    lv_obj_t *Panels[PANEL_MAX];

    lv_obj_t *lbl_Status;
    lv_obj_t *img_Status;

    lv_obj_t *lbl_C_Model;
    lv_obj_t *lbl_C_Version;
    lv_obj_t *lbl_C_SerialNumber;

    lv_obj_t *lbl_I_AC_Energy_WH;
    lv_obj_t *lbl_I_AC_Energy_WH_Last24H;
    lv_obj_t *lbl_I_AC_Power;
    lv_obj_t *lbl_I_AC_Frequency;
    lv_obj_t *lbl_I_Temp_Sink;

    lv_obj_t *chart_Power_24H;
    lv_chart_series_t *chart_Series_24H;

    lv_obj_t *chart_Power_1H;
    lv_chart_series_t *chart_Series_1H;

    lv_obj_t *lbl_I_AC_CurrentA;
    lv_obj_t *lbl_I_AC_CurrentB;
    lv_obj_t *lbl_I_AC_CurrentC;

    lv_obj_t *lbl_I_AC_VoltageAN;
    lv_obj_t *lbl_I_AC_VoltageBN;
    lv_obj_t *lbl_I_AC_VoltageCN;

    lv_obj_t *arc_AmpA;
    lv_obj_t *arc_AmpB;
    lv_obj_t *arc_AmpC;

    lv_obj_t *arc_VoltAN;
    lv_obj_t *arc_VoltBN;
    lv_obj_t *arc_VoltCN;

    SemaphoreHandle_t BackLightChange;
    uint8_t BackLightActive;
    bool *ntp_synced;
} GuiData_t;

typedef enum { GUI_STATE_WIFI_DISCONNECT, GUI_STATE_WIFI_CONNECTED, GUI_STATE_SE_DISCONNECTED, GUI_STATE_SE_CONNECTED, GUI_STATE_TIME } TGuiState;

esp_err_t GUI_Setup(GuiData_t *);

esp_err_t GUI_UpdatePanels(GuiData_t *, SolarEdge_t *);

esp_err_t GUI_SetStatus(GuiData_t *, TGuiState s);

esp_err_t GUI_TogglePanel(GuiData_t *gd);

void SmoothedAverage_24H(SolarEdge_t *sf);

void SmoothedAverage_1H(SolarEdge_t *sf);
