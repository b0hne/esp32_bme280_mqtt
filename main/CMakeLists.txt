idf_component_register(SRCS "app_main.c" "bme280.c"
    INCLUDE_DIRS "."
    REQUIRES driver net_mqtt
)
