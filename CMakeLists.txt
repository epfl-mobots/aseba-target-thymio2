set(CMAKE_TOOLCHAIN_FILE cmake-microchip/toolchain.cmake)
set(MICROCHIP_MCU PIC24FJ128GB106)
cmake_minimum_required(VERSION 3.5)
project(aseba-target-thymio2 VERSION 0.1 LANGUAGES C)

if(NOT ASEBA_DIR)
    message("ASEBA_DIR not set - trying ./aseba")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/aseba/vm/vm.c")
        set(ASEBA_DIR "aseba")
	elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/aseba/aseba/vm/vm.c")
        set(ASEBA_DIR "aseba/aseba")
    else()
        message("ASEBA_DIR not set - trying ../aseba")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../aseba/vm/vm.c")
            set(ASEBA_DIR "../aseba")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../aseba/aseba/vm/vm.c")
            set(ASEBA_DIR "../aseba/aseba")
        endif()
    endif()
endif()
if(NOT ASEBA_DIR)
     message(FATAL_ERROR "ASEBA NOT FOUND")
endif()

message("ASEBA_DIR : ${ASEBA_DIR}")

add_executable(aseba-target-thymio2.cof
    abo.c
    analog.c
    behavior.c
    button.c
    crc.c
    entry.s
    ground_ir.c
    ir_prox.c
    leds.c
    leds_low.s
    log.c
    main.c
    mma7660.c
    mode.c
    motor.c
    ntc.c
    pid_motor.c
    playback.c
    pmp.c
    pwm_motor.c
    rc5.c
    regulator.c
    rf.c
    sd/ff.c
    sd/mmc.c
    sd.c
    sensors.c
    skel-usb.c
    sound.c
    test_mode.c
    thymio-buffer.c
    thymio_natives.c
    tone.c
    usb_uart.c
    wav.c
    #usb
    usb_pic24/usb_descriptors.c
    usb_pic24/usb_device.c
    usb_pic24/usb_function_cdc.c

    #molole
    molole/clock/clock.c
    molole/error/error.c
    molole/flash/flash.c
    molole/i2c/i2c.c
    molole/i2c/master.c
    molole/i2c/master_protocol.c
    molole/i2c/slave.c
    molole/timer/timer.c

    #vm
    ${ASEBA_DIR}/vm/natives.c
    ${ASEBA_DIR}/vm/vm.c
    ${ASEBA_DIR}/transport/buffer/vm-buffer.c
)

#tell cmake to build the assembly files
set_property(SOURCE entry.s PROPERTY LANGUAGE C)
set_property(SOURCE leds_low.s PROPERTY LANGUAGE C)


add_definitions(-DASEBA_LIMITED_MESSAGE_SIZE -DASEBA_ASSERT -DTHYMIO_FIRMWARE_BUILD)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -mlarge-code -fomit-frame-pointer -fno-strict-aliasing -mcpu=${MICROCHIP_MCU_MODEL}  -omf=coff -mafrlcsj")

#Fix a bug when building on linux, is that still needed ?
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-peephole2")


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mlarge-code --stack=16 --check-sections --data-init --pack-data --isr --no-gc-section")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --fill-upper=0 --stackguard=16 --no-force-link --smart-io --report-mem --handles -omf=coff")


include_directories("${CMAKE_CURRENT_SOURCE_DIR}/molole" "${CMAKE_CURRENT_SOURCE_DIR}/usb_pic24" "${CMAKE_CURRENT_SOURCE_DIR}" "${ASEBA_DIR}/vm/" "${ASEBA_DIR}")

set(CMAKE_C_STANDARD_COMPUTED_DEFAULT 90)
include_directories("${MICROCHIP_XC16_PATH}/support/${MICROCHIP_MCU_FAMILY}/"  "${MICROCHIP_XC16_PATH}/support/peripheral_24F")

set(CMAKE_LINKER "${MICROCHIP_XC16_PATH}/bin/xc16-ld")


string(APPEND CMAKE_C_LINK_FLAGS
	" -Wl,--script,p${MICROCHIP_MCU_MODEL}.gld -Wl,-Map=aseba-target-thymio2.map"
)

add_custom_command(TARGET aseba-target-thymio2.cof POST_BUILD COMMAND ${MICROCHIP_XC16_PATH}/bin/xc16-bin2hex -omf=coff $<TARGET_FILE:aseba-target-thymio2.cof>)


# Testing of compilation results
enable_testing()

# Tests: Sanity checks
add_test(NAME sanity-architecture
	COMMAND bash -c "${MICROCHIP_XC16_PATH}/bin/bin/coff-objdump \
		-x $<TARGET_FILE:aseba-target-thymio2.cof> | \
		grep -q '^architecture: 24FJ128GB106'")

add_test(NAME sanity-symboltable
	COMMAND bash -c "${MICROCHIP_XC16_PATH}/bin/bin/coff-nm \
		$<TARGET_FILE:aseba-target-thymio2.cof> | \
		grep -q -w _AsebaVMRun")
