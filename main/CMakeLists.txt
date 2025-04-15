idf_component_register(SRCS "ble_server.cpp" "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver spi_flash
                       REQUIRES bt nvs_flash freertos driver littlefs)