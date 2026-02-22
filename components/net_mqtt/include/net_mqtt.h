#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi STA using CONFIG_WIFI_SSID/CONFIG_WIFI_PASSWORD.
 *        Blocks until an IP address is acquired.
 */
esp_err_t net_wifi_init_sta_blocking(void);

/**
 * @brief Start MQTT client using CONFIG_MQTT_URI (+ optional CONFIG_MQTT_USERNAME/PASSWORD).
 *        Blocks until MQTT is connected.
 *        Also publishes Home Assistant discovery config when connected (if enabled).
 */
esp_err_t net_mqtt_start_blocking(void);

/**
 * @brief Publish a message. If MQTT is not connected, it will block until it is.
 * @param topic Topic string
 * @param payload Null-terminated string payload
 * @param qos 0..2
 * @param retain true/false
 */
esp_err_t net_mqtt_publish_blocking(const char *topic,
                                   const char *payload,
                                   int qos,
                                   bool retain);

#ifdef __cplusplus
}
#endif