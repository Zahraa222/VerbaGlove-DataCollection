idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver spi_flash
                       REQUIRES littlefs)