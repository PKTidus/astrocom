idf_component_register(
    SRCS "main.c" 
    INCLUDE_DIRS "../include"
    REQUIRES esp_event esp_idf_lib_helpers i2cdev
    )