idf_component_register(
    SRCS "astrocom.c" "../src/veml7700.c" "../src/tca9548.c"
    INCLUDE_DIRS "../include"
    REQUIRES esp_event esp_idf_lib_helpers i2cdev
)
