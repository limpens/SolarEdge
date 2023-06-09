#ifndef _espWifi_h_
#define _espWifi_h_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_wifi_types.h>

typedef struct _TSSIDAuth TSSIDAuth;
struct _TSSIDAuth
{
  wifi_auth_mode_t mode;
  const char *identity;
  const char *username;
  const char *password;
};

class espWifi
{
  private:
    typedef void (*esp_wifi_state_callback)(espWifi*);
    typedef esp_err_t (*esp_wifi_auth_callback)(espWifi *, const char *ssid, TSSIDAuth *auth);

    EventGroupHandle_t evg;
    static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    esp_wifi_state_callback cbState; // callback function to signale up/down event
    esp_wifi_auth_callback cbAuth;   // callback function to obtain credentials for network
    void *userptr;

    TaskHandle_t hTask;

    void init_scan_conf(wifi_scan_config_t *);

  public:

    bool connected;

    void SetStateCallback(esp_wifi_state_callback );
    void SetAuthCallback(esp_wifi_auth_callback );
    void SetUserPointer(void *ptr);
    void *GetUserPointer(void);

    espWifi();

    /**
     * @brief This background task will monitor the wifi state and keeps trying to connect
     * 
     * @param ptrEspWifi pointer to this class, to call our own methods
     */
    static void Task(void *ptrEspWifi);
    esp_err_t Init(char *hostname=NULL);
    esp_err_t Up(void);
    esp_err_t Run(void);
    esp_err_t Stop(void);
};

#endif // espWifi.h
