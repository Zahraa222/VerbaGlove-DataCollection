idf_component_register(SRCS "ble_server.c" "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver spi_flash
                       REQUIRES bt nvs_flash freertos littlefs)