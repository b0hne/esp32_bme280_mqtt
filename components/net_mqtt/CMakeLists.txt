idf_component_register(
    SRCS "net_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt
)