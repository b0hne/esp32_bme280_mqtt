/**
 * @file net_mqtt.c
 * @brief WiFi STA + MQTT client helper for ESP-IDF.
 *
 * - Connects WiFi using menuconfig settings
 * - Starts MQTT using menuconfig settings
 * - Auto reconnect (WiFi + MQTT)
 * - Publishes Home Assistant MQTT discovery on connect
 */

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"

#include "mqtt_client.h"

#include "net_mqtt.h"

/* ---- Event bits ---- */
static EventGroupHandle_t s_event_group;
#define WIFI_GOT_IP_BIT     BIT0
#define MQTT_CONNECTED_BIT  BIT1

static esp_mqtt_client_handle_t s_mqtt = NULL;
static esp_netif_t *s_netif = NULL;

/* ---- Helpers ---- */

static void get_node_id(char *out, size_t out_len)
{
    // Use CONFIG_DEVICE_NAME if set, else derive from MAC
    if (strlen(CONFIG_DEVICE_NAME) > 0) {
        snprintf(out, out_len, "%s", CONFIG_DEVICE_NAME);
        return;
    }

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, out_len, "esp32-bme280-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

static void ha_publish_discovery(void)
{
#if CONFIG_HA_DISCOVERY_ENABLE

    char node_id[64];
    get_node_id(node_id, sizeof(node_id));

    // Device info
    char dev_name[96];
    snprintf(dev_name, sizeof(dev_name), "sensor %s", node_id);

    // State topic: use CONFIG_MQTT_STATE_TOPIC
    const char *state_topic = CONFIG_MQTT_STATE_TOPIC;

    // Discovery topics
    // homeassistant/sensor/<nodeid>_<sensor>/config
    char topic[160];
    char payload[1024];
    // Temperature
    snprintf(topic, sizeof(topic), "%s/sensor/%s_temperature/config",
             CONFIG_HA_DISCOVERY_PREFIX, node_id);

    int n = snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"%s Temperature\","
             "\"uniq_id\":\"%s_temperature\","
             "\"stat_t\":\"%s\","
             "\"unit_of_meas\":\"°C\","
             "\"dev_cla\":\"temperature\","
             "\"val_tpl\":\"{{ value_json.temperature }}\","
             "\"dev\":{"
                "\"ids\":[\"%s\"],"
                "\"name\":\"%s\","
                "\"mdl\":\"BME280\","
                "\"mf\":\"Bosch\""
             "}"
             "}",
             dev_name, node_id, state_topic,
             node_id, dev_name);
    if (n > 0 && n < (int)sizeof(payload)) {
        esp_mqtt_client_publish(s_mqtt, topic, payload, 0, 1, true);
    }
    // Pressure
    snprintf(topic, sizeof(topic), "%s/sensor/%s_pressure/config",
             CONFIG_HA_DISCOVERY_PREFIX, node_id);

    n = snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"%s Pressure\","
             "\"uniq_id\":\"%s_pressure\","
             "\"stat_t\":\"%s\","
             "\"unit_of_meas\":\"hPa\","
             "\"dev_cla\":\"pressure\","
             "\"val_tpl\":\"{{ value_json.pressure }}\","
             "\"dev\":{"
                "\"ids\":[\"%s\"],"
                "\"name\":\"%s\","
                "\"mdl\":\"BME280\","
                "\"mf\":\"Bosch\""
             "}"
             "}",
             dev_name, node_id, state_topic,
             node_id, dev_name);
    if (n > 0 && n < (int)sizeof(payload)) {
        esp_mqtt_client_publish(s_mqtt, topic, payload, 0, 1, true);
    }
    // Humidity
    snprintf(topic, sizeof(topic), "%s/sensor/%s_humidity/config",
             CONFIG_HA_DISCOVERY_PREFIX, node_id);

    n = snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"%s Humidity\","
             "\"uniq_id\":\"%s_humidity\","
             "\"stat_t\":\"%s\","
             "\"unit_of_meas\":\"%%\","
             "\"dev_cla\":\"humidity\","
             "\"val_tpl\":\"{{ value_json.humidity }}\","
             "\"dev\":{"
                "\"ids\":[\"%s\"],"
                "\"name\":\"%s\","
                "\"mdl\":\"BME280\","
                "\"mf\":\"Bosch\""
             "}"
             "}",
             dev_name, node_id, state_topic,
             node_id, dev_name);
    if (n > 0 && n < (int)sizeof(payload)) {
        esp_mqtt_client_publish(s_mqtt, topic, payload, 0, 1, true);
    }
#endif
}

/* ---- Event handlers ---- */

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Clear IP bit and reconnect
        xEventGroupClearBits(s_event_group, WIFI_GOT_IP_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_event_group, WIFI_GOT_IP_BIT);
    }
}

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            xEventGroupSetBits(s_event_group, MQTT_CONNECTED_BIT);
            ha_publish_discovery();
            break;

        case MQTT_EVENT_DISCONNECTED:
            xEventGroupClearBits(s_event_group, MQTT_CONNECTED_BIT);
            break;

        default:
            break;
    }
}

/* ---- Public API ---- */

esp_err_t net_wifi_init_sta_blocking(void)
{
    if (!s_event_group) {
        s_event_group = xEventGroupCreate();
    }

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid,
            CONFIG_WIFI_SSID,
            sizeof(wifi_config.sta.ssid));

    strncpy((char *)wifi_config.sta.password,
            CONFIG_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // wait for IP
    xEventGroupWaitBits(s_event_group, WIFI_GOT_IP_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t net_mqtt_start_blocking(void)
{
    if (!s_event_group) {
        s_event_group = xEventGroupCreate();
    }

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_URI,
        // esp-mqtt does auto reconnect by default
    };

    if (strlen(CONFIG_MQTT_USERNAME) > 0) {
        cfg.credentials.username = CONFIG_MQTT_USERNAME;
    }
    if (strlen(CONFIG_MQTT_PASSWORD) > 0) {
        cfg.credentials.authentication.password = CONFIG_MQTT_PASSWORD;
    }

    s_mqtt = esp_mqtt_client_init(&cfg);
    if (!s_mqtt) return ESP_FAIL;

    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt));

    // wait until connected
    xEventGroupWaitBits(s_event_group, MQTT_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t net_mqtt_publish_blocking(const char *topic,
                                   const char *payload,
                                   int qos,
                                   bool retain)
{
    if (!s_mqtt) return ESP_ERR_INVALID_STATE;

    // wait until connected (auto reconnect will set bit again)
    xEventGroupWaitBits(s_event_group, MQTT_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    int msg_id = esp_mqtt_client_publish(s_mqtt,
                                        topic,
                                        payload,
                                        0,
                                        qos,
                                        retain);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}