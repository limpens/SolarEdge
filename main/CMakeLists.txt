#
# The base source and components to connect to the SolarEdge device and 
# translate to json objects for mqtt:
#
set(SE_SOURCES main.cpp wifi.cpp modbus.cpp TaskModbus.cpp espWifi.cpp Configuration.cpp)
set(SE_COMPONENTS json mqtt esp_wifi wpa_supplicant nvs_flash console esp_partition app_update)

#
# Additional code to display values semi-realtime on LCD display:
#
if(CONFIG_SOLAREDGE_USE_LCD)
  list(APPEND SE_SOURCES se_logo.c lcd.cpp gui.cpp)
  list(APPEND SE_COMPONENTS lvgl esp_lcd driver esp_lcd_touch_gt911)
endif()


idf_component_register(
  SRCS ${SE_SOURCES}
  INCLUDE_DIRS .
  REQUIRES ${SE_COMPONENTS}
)
