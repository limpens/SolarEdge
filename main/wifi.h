#pragma once

esp_err_t WifiStart(void *);

typedef struct
{
    bool online;
    char *ssid;
    char *wpa2_user;
    char *wpa2_ident;
    char *password;
} WifiUser_t;
