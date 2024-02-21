idf_component_register(
    SRCS "example_light_measure.c" "../src/veml7700.c" "../src/tca9548.c"
    INCLUDE_DIRS "../include"
    REQUIRES esp_event esp_idf_lib_helpers i2cdev
)
