#include "wifi.h"
#include "passwords.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <time.h>
#include "esp_sntp.h"

/************************************************************************/
/*
This is all event driven.
initialize the network stack,
register event handlers,
start the WiFi driver,
and react to asynchronous events such as
STA_START,
STA_DISCONNECTED,
and STA_GOT_IP.
*/


#define ESP_MAXIMUM_RETRY  5

static void obtain_time_wifi(void);
static volatile bool s_wifi_connected = false;
static bool s_sntp_initialized = false;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;
static volatile bool s_time_synced = false;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
        
        //disconnect reason log
        wifi_event_sta_disconnected_t *disc =
        (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Disconnect reason: %d", disc->reason);

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_connected = true;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,

#ifdef CONFIG_ESP_WIFI_WPA3_COMPATIBLE_SUPPORT
            .disable_wpa3_compatible_mode = 0,
#endif
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:(wifi) password:(password)",
                 WIFI_SSID, WIFI_PASS);
        ESP_LOGI(TAG, "WiFi connection is good");
        obtain_time_wifi();
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:(wifi), password:(password)",
                 WIFI_SSID, WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void sntp_sync_cb(struct timeval *tv)
{
    s_time_synced = true;
}

static void obtain_time_wifi(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");

    if (!s_sntp_initialized) {
        setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
        s_time_synced = false; //dont trust value until guaranteed
        tzset();
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_set_sync_interval(2 * 60 * 1000); // 2 minutes (was 1 hour)
        esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);
        esp_sntp_init();
        s_sntp_initialized = true;
    }

    time_t now = 0;
    struct tm timeinfo = {0};

    int retry = 0;
    const int retry_count = 15;

    while (timeinfo.tm_year < (2024 - 1900) && retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry + 1, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));

        time(&now);
        localtime_r(&now, &timeinfo);
        retry++;
    }

    if (timeinfo.tm_year >= (2024 - 1900)) {
        char strftime_buf[64];

        s_time_synced = 1;

        time(&now);
        localtime_r(&now, &timeinfo);

        strftime(strftime_buf, sizeof(strftime_buf), "%A, %B %d %Y %I:%M:%S %p", &timeinfo);
        ESP_LOGI(TAG, "Current time: %s", strftime_buf);
    } else {
        ESP_LOGW(TAG, "Failed to obtain time from SNTP");
    }
}

void wifi_init(void){
    //temp wifi logic
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI("TAG", "Starting WiFi test");
    wifi_init_sta();
    ESP_LOGI("TAG", "WiFi test complete");
}

bool wifi_is_connected(void){
    return s_wifi_connected;
}

bool wifi_time_is_synced(void)
{
    return s_time_synced;
}

void wifi_reconnect_task(void *arg) {
    while (1) {
        if (s_wifi_connected && s_sntp_initialized) {
            vTaskDelay(pdMS_TO_TICKS(24UL * 60 * 60 * 1000)); // all good, check again tomorrow
        } else if (s_wifi_connected && !s_sntp_initialized) {
            obtain_time_wifi(); // connected but SNTP never started (boot failure case)
        } else {
            vTaskDelay(pdMS_TO_TICKS(10000));
            if (s_retry_num >= ESP_MAXIMUM_RETRY) {
                s_retry_num = 0;
                esp_wifi_connect();
            }
        }
    }
}
