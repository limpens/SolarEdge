#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_log.h>

#include "lcd.h"
#include "gui.h"

#include "Smooth.h"

#define TAG "gui"

LV_IMG_DECLARE(se_logo);
LV_IMG_DECLARE(se_state_1);
LV_IMG_DECLARE(se_state_4);
LV_IMG_DECLARE(se_state_5);

#define NUM_MEASUREMENTS 60
#define PANEL_HEIGHT     354
#define PANEL_OFFSET     104
#define CHART_OFFSET     -16
#define CHART_WIDTH      640
#define CHART_HEIGHT     (PANEL_HEIGHT + CHART_OFFSET)
#define NUM_TICKS_X_24H  12
#define NUM_TICKS_X_1H   6

void WattToUnits(char *buf, double watts)
{
#define NUM_UNITS 6
    const char *units[NUM_UNITS] = { "W", "kW", "MW", "GW", "TW", "PW" };
    int unitIndex = 0;

    while (watts >= 1000 && unitIndex < (NUM_UNITS - 1))
    {
        watts /= 1000;
        unitIndex++;
    }

    sprintf(buf, "%.2f %s", watts, units[unitIndex]);
}

void ui_event_Screen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    GuiData_t *gd = (GuiData_t *)lv_event_get_user_data(e);

    if (event_code == LV_EVENT_CLICKED)
    {
        if (gd->BackLightActive)
            GUI_TogglePanel(gd);
        else
        {
            gd->BackLightActive = 30;
            xSemaphoreGive(gd->BackLightChange);
        }
    }
}

static void DrawGradient(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = (lv_obj_draw_part_dsc_t *)lv_event_get_param(e);

    if (dsc->part == LV_PART_ITEMS)
    {
        if (!dsc->p1 || !dsc->p2)
            return;

        lv_obj_t *obj = lv_event_get_target(e);

        lv_draw_mask_line_param_t line_mask_param;
        lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y, LV_DRAW_MASK_LINE_SIDE_BOTTOM);
        int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

        lv_coord_t h = lv_obj_get_height(obj);
        lv_draw_mask_fade_param_t fade_mask_param;
        lv_draw_mask_fade_init(&fade_mask_param, &obj->coords, LV_OPA_COVER, obj->coords.y1 + h / 8, LV_OPA_40, obj->coords.y2);
        int16_t fade_mask_id = lv_draw_mask_add(&fade_mask_param, NULL);

        lv_draw_rect_dsc_t draw_rect_dsc;
        lv_draw_rect_dsc_init(&draw_rect_dsc);
        draw_rect_dsc.bg_opa = LV_OPA_40;
        draw_rect_dsc.bg_color = dsc->line_dsc->color;

        lv_area_t a;
        a.x1 = dsc->p1->x;
        a.x2 = dsc->p2->x - 1;
        a.y1 = LV_MIN(dsc->p1->y, dsc->p2->y);
        a.y2 = obj->coords.y2;
        lv_draw_rect(dsc->draw_ctx, &draw_rect_dsc, &a);

        lv_draw_mask_free_param(&line_mask_param);
        lv_draw_mask_free_param(&fade_mask_param);
        lv_draw_mask_remove_id(line_mask_id);
        lv_draw_mask_remove_id(fade_mask_id);
    }
}

static void draw_event_cb_24H(lv_event_t *e)
{
    static char *hours[NUM_TICKS_X_24H] = { (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"00:00",
        (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"now" };

    lv_obj_draw_part_dsc_t *dsc = (lv_obj_draw_part_dsc_t *)lv_event_get_param(e);

    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X)
    {
        time_t now = time(NULL);
        if (dsc->value != NUM_TICKS_X_24H - 1)
        {
            time_t past = now - (3600 * (22 - (dsc->value * 2)));
            struct tm *ltm = localtime(&past);
            strftime(hours[dsc->value], 6, "%H:%M", ltm);
        }
        dsc->text = hours[dsc->value];
    }

    DrawGradient(e);
}

static void draw_event_cb_1H(lv_event_t *e)
{
    static char *hours[NUM_TICKS_X_1H] = { (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"00:00", (char *)"now" };

    lv_obj_draw_part_dsc_t *dsc = (lv_obj_draw_part_dsc_t *)lv_event_get_param(e);

    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X)
    {
        time_t now = time(NULL);
        if (dsc->value != NUM_TICKS_X_1H - 1)
        {
            time_t past = now - (300 * (10 - dsc->value * 2));
            struct tm *ltm = localtime(&past);
            strftime(hours[dsc->value], 6, "%H:%M", ltm);
        }
        dsc->text = hours[dsc->value];
    }

    DrawGradient(e);
}

esp_err_t GUI_Setup(GuiData_t *data)
{
    lvgl_acquire();

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_event_cb(screen, ui_event_Screen, LV_EVENT_ALL, data);

    data->Panels[PANEL_CHART_1H] = lv_obj_create(screen);
    data->Panels[PANEL_CHART_24H] = lv_obj_create(screen);
    data->Panels[PANEL_GAUGE] = lv_obj_create(screen);
    data->Panels[PANEL_MAX] = nullptr;

    lv_obj_set_width(data->Panels[PANEL_CHART_1H], 800);
    lv_obj_set_height(data->Panels[PANEL_CHART_1H], PANEL_HEIGHT);
    lv_obj_set_align(data->Panels[PANEL_CHART_1H], LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(data->Panels[PANEL_CHART_1H], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(data->Panels[PANEL_CHART_1H], 0, PANEL_OFFSET);
    lv_obj_set_style_bg_color(data->Panels[PANEL_CHART_1H], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(data->Panels[PANEL_CHART_1H], 0, LV_PART_MAIN);

    lv_obj_set_width(data->Panels[PANEL_CHART_24H], 800);
    lv_obj_set_height(data->Panels[PANEL_CHART_24H], PANEL_HEIGHT);
    lv_obj_set_align(data->Panels[PANEL_CHART_24H], LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(data->Panels[PANEL_CHART_24H], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(data->Panels[PANEL_CHART_24H], 0, PANEL_OFFSET);
    lv_obj_set_style_bg_color(data->Panels[PANEL_CHART_24H], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(data->Panels[PANEL_CHART_24H], 0, LV_PART_MAIN);

    lv_obj_set_width(data->Panels[PANEL_GAUGE], 800);
    lv_obj_set_height(data->Panels[PANEL_GAUGE], PANEL_HEIGHT);
    lv_obj_set_align(data->Panels[PANEL_GAUGE], LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(data->Panels[PANEL_GAUGE], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(data->Panels[PANEL_GAUGE], 0, PANEL_OFFSET);
    lv_obj_set_style_bg_color(data->Panels[PANEL_GAUGE], lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(data->Panels[PANEL_GAUGE], 0, LV_PART_MAIN);

    lv_obj_t *ui_logo_SE = lv_img_create(screen);
    lv_img_set_src(ui_logo_SE, &se_logo);
    lv_obj_set_align(ui_logo_SE, LV_ALIGN_TOP_MID);

    lv_obj_t *ui_Label_Version = lv_label_create(screen);
    lv_obj_set_style_text_font(ui_Label_Version, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(ui_Label_Version, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Label_Version, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_Label_Version, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(ui_Label_Version, _PROJECT_VER_);
    lv_obj_set_pos(ui_Label_Version, -20, 0);

    data->img_Status = lv_img_create(screen);
    lv_obj_set_align(data->img_Status, LV_ALIGN_TOP_RIGHT);
    lv_img_set_src(data->img_Status, &se_state_1);

    data->lbl_C_Model = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_C_Model, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_C_Model, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_C_Model, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_C_Model, LV_ALIGN_TOP_LEFT);
    lv_label_set_text(data->lbl_C_Model, "");

    data->lbl_C_Version = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_C_Version, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_C_Version, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_C_Version, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_C_Version, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(data->lbl_C_Version, 0, 14);
    lv_label_set_text(data->lbl_C_Version, "");

    data->lbl_C_SerialNumber = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_C_SerialNumber, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_C_SerialNumber, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_C_SerialNumber, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_C_SerialNumber, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(data->lbl_C_SerialNumber, 0, 28);
    lv_label_set_text(data->lbl_C_SerialNumber, "");

    data->lbl_Status = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_Status, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_Status, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_Status, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_Status, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(data->lbl_Status, 0, 48);
    lv_label_set_text(data->lbl_Status, "");

    data->lbl_I_AC_Energy_WH = lv_label_create(screen);
    lv_obj_set_width(data->lbl_I_AC_Energy_WH, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_Energy_WH, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_Energy_WH, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(data->lbl_I_AC_Energy_WH, "");

    data->lbl_I_AC_Power = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_I_AC_Power, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_Power, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_Power, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_Power, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(data->lbl_I_AC_Power, -128, 64);
    lv_obj_set_style_text_color(data->lbl_I_AC_Power, LV_COLOR_MAKE16(0xd2, 0xe3, 0x36), 0);
    lv_label_set_text(data->lbl_I_AC_Power, "");

    data->lbl_I_AC_Energy_WH_Last24H = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_I_AC_Energy_WH_Last24H, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_Energy_WH_Last24H, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_Energy_WH_Last24H, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_Energy_WH_Last24H, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(data->lbl_I_AC_Energy_WH_Last24H, 128, 64);
    lv_obj_set_style_text_color(data->lbl_I_AC_Energy_WH_Last24H, LV_COLOR_MAKE16(0xd2, 0xe3, 0x36), 0);
    lv_label_set_text(data->lbl_I_AC_Energy_WH_Last24H, "");

    data->lbl_I_AC_Frequency = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_I_AC_Frequency, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_Frequency, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_Frequency, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_Frequency, LV_ALIGN_BOTTOM_LEFT);
    lv_label_set_text(data->lbl_I_AC_Frequency, "Freq: ??.?? Hz");

    data->lbl_I_Temp_Sink = lv_label_create(screen);
    lv_obj_set_style_text_font(data->lbl_I_Temp_Sink, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_Temp_Sink, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_Temp_Sink, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_Temp_Sink, LV_ALIGN_BOTTOM_RIGHT);
    lv_label_set_text(data->lbl_I_Temp_Sink, "Temp: ??.?? °C");

    data->chart_Power_1H = lv_chart_create(data->Panels[PANEL_CHART_1H]);
    lv_obj_set_width(data->chart_Power_1H, CHART_WIDTH);
    lv_obj_set_height(data->chart_Power_1H, CHART_HEIGHT);
    lv_obj_set_align(data->chart_Power_1H, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_pos(data->chart_Power_1H, 0, CHART_OFFSET);

    lv_chart_set_type(data->chart_Power_1H, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(data->chart_Power_1H, CHART_1H_NUM_POINTS);
    lv_obj_set_style_line_width(data->chart_Power_1H, 1, LV_PART_ITEMS);
    lv_obj_set_style_size(data->chart_Power_1H, 2, LV_PART_INDICATOR);

    data->chart_Series_1H = lv_chart_add_series(data->chart_Power_1H, lv_palette_main(LV_PALETTE_LIGHT_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_set_range(data->chart_Power_1H, LV_CHART_AXIS_PRIMARY_Y, 0, 6000);
    lv_chart_set_axis_tick(data->chart_Power_1H, LV_CHART_AXIS_PRIMARY_X, 10, 0, NUM_TICKS_X_1H, 1, true, 80);
    lv_chart_set_axis_tick(data->chart_Power_1H, LV_CHART_AXIS_PRIMARY_Y, 5, 0, 5, 1, true, 50);

    lv_obj_add_event_cb(data->chart_Power_1H, draw_event_cb_1H, LV_EVENT_DRAW_PART_BEGIN, NULL);

    data->chart_Power_24H = lv_chart_create(data->Panels[PANEL_CHART_24H]);
    lv_obj_set_width(data->chart_Power_24H, CHART_WIDTH);
    lv_obj_set_height(data->chart_Power_24H, CHART_HEIGHT);
    lv_obj_set_align(data->chart_Power_24H, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_pos(data->chart_Power_24H, 0, CHART_OFFSET);

    lv_chart_set_type(data->chart_Power_24H, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(data->chart_Power_24H, CHART_24H_NUM_POINTS);
    lv_obj_set_style_line_width(data->chart_Power_24H, 1, LV_PART_ITEMS);
    lv_obj_set_style_size(data->chart_Power_24H, 2, LV_PART_INDICATOR);

    data->chart_Series_24H = lv_chart_add_series(data->chart_Power_24H, lv_palette_main(LV_PALETTE_LIGHT_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_set_range(data->chart_Power_24H, LV_CHART_AXIS_PRIMARY_Y, 0, 6000);
    lv_chart_set_axis_tick(data->chart_Power_24H, LV_CHART_AXIS_PRIMARY_X, 10, 0, NUM_TICKS_X_24H, 1, true, 80);
    lv_chart_set_axis_tick(data->chart_Power_24H, LV_CHART_AXIS_PRIMARY_Y, 5, 0, 5, 1, true, 50);

    lv_obj_add_event_cb(data->chart_Power_24H, draw_event_cb_24H, LV_EVENT_DRAW_PART_BEGIN, NULL);

    data->arc_AmpA = lv_arc_create(data->Panels[PANEL_GAUGE]);
    lv_obj_set_width(data->arc_AmpA, 150);
    lv_obj_set_height(data->arc_AmpA, 150);
    lv_obj_set_align(data->arc_AmpA, LV_ALIGN_BOTTOM_LEFT);
    lv_arc_set_range(data->arc_AmpA, 0, 16);
    lv_obj_set_style_bg_opa(data->arc_AmpA, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(data->arc_AmpA, 8, LV_PART_INDICATOR);

    data->lbl_I_AC_CurrentA = lv_label_create(data->arc_AmpA);
    lv_obj_set_style_text_font(data->lbl_I_AC_CurrentA, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_CurrentA, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_CurrentA, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_CurrentA, LV_ALIGN_CENTER);
    lv_label_set_text(data->lbl_I_AC_CurrentA, "??.?? Amp");

    data->arc_AmpB = lv_arc_create(data->Panels[PANEL_GAUGE]);
    lv_obj_set_width(data->arc_AmpB, 150);
    lv_obj_set_height(data->arc_AmpB, 150);
    lv_obj_set_align(data->arc_AmpB, LV_ALIGN_BOTTOM_MID);
    lv_arc_set_range(data->arc_AmpB, 0, 16);
    lv_obj_set_style_bg_opa(data->arc_AmpB, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(data->arc_AmpB, 8, LV_PART_INDICATOR);

    data->lbl_I_AC_CurrentB = lv_label_create(data->arc_AmpB);
    lv_obj_set_style_text_font(data->lbl_I_AC_CurrentB, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_CurrentB, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_CurrentB, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_CurrentB, LV_ALIGN_CENTER);
    lv_label_set_text(data->lbl_I_AC_CurrentB, "??.?? Amp");

    data->arc_AmpC = lv_arc_create(data->Panels[PANEL_GAUGE]);
    lv_obj_set_width(data->arc_AmpC, 150);
    lv_obj_set_height(data->arc_AmpC, 150);
    lv_obj_set_align(data->arc_AmpC, LV_ALIGN_BOTTOM_RIGHT);
    lv_arc_set_range(data->arc_AmpC, 0, 16);
    lv_obj_set_style_bg_opa(data->arc_AmpC, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(data->arc_AmpC, 8, LV_PART_INDICATOR);

    data->lbl_I_AC_CurrentC = lv_label_create(data->arc_AmpC);
    lv_obj_set_style_text_font(data->lbl_I_AC_CurrentC, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_CurrentC, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_CurrentC, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_CurrentC, LV_ALIGN_CENTER);
    lv_label_set_text(data->lbl_I_AC_CurrentC, "??.?? Amp");

    data->arc_VoltAN = lv_arc_create(data->Panels[PANEL_GAUGE]);
    lv_obj_set_width(data->arc_VoltAN, 150);
    lv_obj_set_height(data->arc_VoltAN, 150);
    lv_obj_set_align(data->arc_VoltAN, LV_ALIGN_BOTTOM_LEFT);
    lv_arc_set_range(data->arc_VoltAN, 0, 255);
    lv_obj_set_style_bg_opa(data->arc_VoltAN, 10, LV_STATE_DEFAULT);
    lv_obj_set_pos(data->arc_VoltAN, 0, -160);
    lv_obj_set_style_arc_width(data->arc_VoltAN, 8, LV_PART_INDICATOR);

    data->lbl_I_AC_VoltageAN = lv_label_create(data->arc_VoltAN);
    lv_obj_set_style_text_font(data->lbl_I_AC_VoltageAN, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_VoltageAN, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_VoltageAN, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_VoltageAN, LV_ALIGN_CENTER);
    lv_label_set_text(data->lbl_I_AC_VoltageAN, "???.?? Volt");

    data->arc_VoltBN = lv_arc_create(data->Panels[PANEL_GAUGE]);
    lv_obj_set_width(data->arc_VoltBN, 150);
    lv_obj_set_height(data->arc_VoltBN, 150);
    lv_obj_set_align(data->arc_VoltBN, LV_ALIGN_BOTTOM_MID);
    lv_arc_set_range(data->arc_VoltBN, 0, 255);
    lv_obj_set_style_bg_opa(data->arc_VoltBN, 10, LV_STATE_DEFAULT);
    lv_obj_set_pos(data->arc_VoltBN, 0, -160);
    lv_obj_set_style_arc_width(data->arc_VoltBN, 8, LV_PART_INDICATOR);

    data->lbl_I_AC_VoltageBN = lv_label_create(data->arc_VoltBN);
    lv_obj_set_style_text_font(data->lbl_I_AC_VoltageBN, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_VoltageBN, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_VoltageBN, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_VoltageBN, LV_ALIGN_CENTER);
    lv_label_set_text(data->lbl_I_AC_VoltageBN, "???.?? Volt");

    data->arc_VoltCN = lv_arc_create(data->Panels[PANEL_GAUGE]);
    lv_obj_set_width(data->arc_VoltCN, 150);
    lv_obj_set_height(data->arc_VoltCN, 150);
    lv_obj_set_align(data->arc_VoltCN, LV_ALIGN_BOTTOM_RIGHT);
    lv_arc_set_range(data->arc_VoltCN, 0, 255);
    lv_obj_set_style_bg_opa(data->arc_VoltCN, 10, LV_STATE_DEFAULT);
    lv_obj_set_pos(data->arc_VoltCN, 0, -160);
    lv_obj_set_style_arc_width(data->arc_VoltCN, 8, LV_PART_INDICATOR);

    data->lbl_I_AC_VoltageCN = lv_label_create(data->arc_VoltCN);
    lv_obj_set_style_text_font(data->lbl_I_AC_VoltageCN, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_width(data->lbl_I_AC_VoltageCN, LV_SIZE_CONTENT);
    lv_obj_set_height(data->lbl_I_AC_VoltageCN, LV_SIZE_CONTENT);
    lv_obj_set_align(data->lbl_I_AC_VoltageCN, LV_ALIGN_CENTER);
    lv_label_set_text(data->lbl_I_AC_VoltageCN, "???.?? Volt");

    lv_obj_clear_flag(data->Panels[PANEL_CHART_1H], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->Panels[PANEL_CHART_24H], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(data->Panels[PANEL_GAUGE], LV_OBJ_FLAG_HIDDEN);

    lv_disp_load_scr(screen);

    lvgl_release();

    return ESP_OK;
}

esp_err_t GUI_UpdatePanels(GuiData_t *gd, SolarEdge_t *se)
{
    char buf[32];

    SmoothedAverage_24H(se);
    SmoothedAverage_1H(se);

    lvgl_acquire();

    lv_label_set_text(gd->lbl_C_Model, (const char *)se->C_Model);
    lv_label_set_text(gd->lbl_C_Version, (const char *)se->C_Version);
    lv_label_set_text(gd->lbl_C_SerialNumber, (const char *)se->C_SerialNumber);

    snprintf(buf, sizeof(buf), "Total: ");
    WattToUnits(buf + strlen(buf), se->I_AC_Energy_WH);
    lv_label_set_text(gd->lbl_I_AC_Energy_WH, buf);

    WattToUnits(buf, (se->I_AC_Energy_WH - se->I_AC_Energy_WH_Last24H));
    lv_label_set_text(gd->lbl_I_AC_Energy_WH_Last24H, buf);

    WattToUnits(buf, se->I_AC_Power);
    lv_label_set_text(gd->lbl_I_AC_Power, buf);

    snprintf(buf, sizeof(buf), "Freq: %.2f Hz", se->I_AC_Frequency);
    lv_label_set_text(gd->lbl_I_AC_Frequency, buf);

    snprintf(buf, sizeof(buf), "Temp: %.2f °C", se->I_Temp_Sink);
    lv_label_set_text(gd->lbl_I_Temp_Sink, buf);

    snprintf(buf, sizeof(buf), "%2.2f Amp", se->I_AC_CurrentA);
    lv_label_set_text(gd->lbl_I_AC_CurrentA, buf);

    snprintf(buf, sizeof(buf), "%2.2f Amp", se->I_AC_CurrentB);
    lv_label_set_text(gd->lbl_I_AC_CurrentB, buf);

    snprintf(buf, sizeof(buf), "%2.2f Amp", se->I_AC_CurrentC);
    lv_label_set_text(gd->lbl_I_AC_CurrentC, buf);

    lv_arc_set_value(gd->arc_AmpA, se->I_AC_CurrentA);
    lv_arc_set_value(gd->arc_AmpB, se->I_AC_CurrentB);
    lv_arc_set_value(gd->arc_AmpC, se->I_AC_CurrentC);

    snprintf(buf, sizeof(buf), "%3.2f Volt", se->I_AC_VoltageAN);
    lv_label_set_text(gd->lbl_I_AC_VoltageAN, buf);

    snprintf(buf, sizeof(buf), "%3.2f Volt", se->I_AC_VoltageBN);
    lv_label_set_text(gd->lbl_I_AC_VoltageBN, buf);

    snprintf(buf, sizeof(buf), "%3.2f Volt", se->I_AC_VoltageCN);
    lv_label_set_text(gd->lbl_I_AC_VoltageCN, buf);

    lv_arc_set_value(gd->arc_VoltAN, se->I_AC_VoltageAN);
    lv_arc_set_value(gd->arc_VoltBN, se->I_AC_VoltageBN);
    lv_arc_set_value(gd->arc_VoltCN, se->I_AC_VoltageCN);

    if (se->I_Status == 5)
        lv_img_set_src(gd->img_Status, &se_state_5);
    else if (se->I_Status == 4)
        lv_img_set_src(gd->img_Status, &se_state_4);
    else
        lv_img_set_src(gd->img_Status, &se_state_1);

    if (se->UpdatePower_24H)
    {
        lv_chart_set_next_value(gd->chart_Power_24H, gd->chart_Series_24H, se->AVG_Power_24H);
        se->UpdatePower_24H = false;
    }

    if (se->UpdatePower_1H)
    {
        lv_chart_set_next_value(gd->chart_Power_1H, gd->chart_Series_1H, se->AVG_Power_1H);
        se->UpdatePower_1H = false;
    }

    lv_chart_refresh(gd->chart_Power_24H);
    lv_chart_refresh(gd->chart_Power_1H);

    lvgl_release();

    return ESP_OK;
}

esp_err_t GUI_TogglePanel(GuiData_t *gd)
{
    static uint8_t toggle;

    toggle++;
    if (toggle >= PANEL_MAX)
        toggle = 0;

    lvgl_acquire();

    for (uint8_t i = 0; i != PANEL_MAX; i++)
    {
        if (gd->Panels[i])
        {
            if (i != toggle)
                lv_obj_add_flag(gd->Panels[i], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_clear_flag(gd->Panels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lvgl_release();

    return ESP_OK;
}

esp_err_t GUI_SetStatus(GuiData_t *gd, TGuiState s)
{
    lvgl_acquire();

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
            if (gd->ntp_synced)
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

    lvgl_release();

    return ESP_OK;
}

void SmoothedAverage_24H(SolarEdge_t *sf)
{
    static Smooth avg(NUM_MEASUREMENTS);
    static time_t lastTime = 0;

    avg += sf->I_AC_Power;
    if (lastTime < time(NULL))
    {
        if (sf->UpdatePower_24H == false)
        {
            lastTime = time(NULL) + 360;
            sf->AVG_Power_24H = avg();
            sf->UpdatePower_24H = true;
        }
    }
}

void SmoothedAverage_1H(SolarEdge_t *sf)
{
    static Smooth avg(NUM_MEASUREMENTS);
    static time_t lastTime = 0;

    avg += sf->I_AC_Power;
    if (lastTime < time(NULL))
    {
        if (sf->UpdatePower_1H == false)
        {
            lastTime = time(NULL) + 10;
            sf->AVG_Power_1H = avg();
            sf->UpdatePower_1H = true;
        }
    }
}
