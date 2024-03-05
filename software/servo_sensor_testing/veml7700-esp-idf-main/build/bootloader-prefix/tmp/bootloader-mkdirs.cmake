# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/binhp/esp/v5.2/esp-idf/components/bootloader/subproject"
  "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader"
  "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader-prefix"
  "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader-prefix/tmp"
  "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader-prefix/src"
  "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/binhp/esp/v5.2/servo_sensor_testing/veml7700-esp-idf-main/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
