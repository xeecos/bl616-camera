cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

sdk_add_include_directories(../src)

target_sources(app PRIVATE 
    ../src/microphone.c
    ../src/led.c
    ../src/cam.c
    ../src/wifi.c
    ../src/service.c
    ../src/web/mlwip_https.c
    ../src/usb_uart.c
    ../src/firmata.c
    ../src/uart.c
    ../src/i2c.c
    ../src/jpeg_head.c
)
sdk_set_main_file(../src/main.c)

project(bl618_camera)
