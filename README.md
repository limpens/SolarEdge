SolarEdge
====

Two targets are available: ESP32 and ESP32S3


* ESP32:
  
  Headless esp32 with a tcp connection to the SolarEdge inverter and submitting json messages over MQTT

      rm sdkconfig
      idf.py fullclean
      idf.py set-target esp32
      idf.py menuconfig - fill in the requirements for the SolarEdge project -
      idf.py build flash monitor


* ESP32S3:

  Uses [Sunton ESP32-S3 800x480 Capacitive touch display](https://nl.aliexpress.com/item/1005004788147691.html) to display semi-realtime stats and performs the same functionality as the ESP32

      rm sdkconfig
      idf.py fullclean
      idf.py set-target esp32s3
      idf.py menuconfig  - fill in the requirements for the SolarEdge project -
      idf.py build flash monitor

  * Uses ESP-IDF component (esp_lcd_panel_rgb) for the 16-bit parallel display
  * Uses ESP-IDF component (esp_lcd_touch_gt911) for the touch layer
