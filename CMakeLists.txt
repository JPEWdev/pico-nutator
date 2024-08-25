cmake_minimum_required(VERSION 3.13)

include(pico-sdk/pico_sdk_init.cmake)

project(nutator)

pico_sdk_init()

add_compile_options(-Wall -Werror)

add_executable(nutator
    src/main.c
    src/stepper-motor.c
    src/nhd-k3z.c
    src/button.c
    src/persist.c
)

target_link_libraries(nutator pico_stdlib hardware_gpio hardware_pwm)
pico_set_linker_script(nutator ${CMAKE_SOURCE_DIR}/src/memmap.ld)
pico_enable_stdio_usb(nutator 1)
pico_enable_stdio_uart(nutator 0)

pico_add_extra_outputs(nutator)

