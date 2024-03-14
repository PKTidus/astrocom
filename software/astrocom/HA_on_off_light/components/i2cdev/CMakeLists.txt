idf_component_register(
    SRCS "i2cdev.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_idf_lib_helpers
)
