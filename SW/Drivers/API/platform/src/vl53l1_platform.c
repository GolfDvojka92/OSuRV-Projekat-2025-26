/*******************************************************************************
 * Linux I2C Platform Implementation for VL53L1X
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "vl53l1_platform.h"
#include "vl53l1_platform_log.h"
#include "vl53l1_api.h"

#define I2C_DEVICE "/dev/i2c-1"

static int i2c_fd = -1;

#define trace_print(level, ...) \
    _LOG_TRACE_PRINT(VL53L1_TRACE_MODULE_PLATFORM, \
    level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)

#define trace_i2c(...) \
    _LOG_TRACE_PRINT(VL53L1_TRACE_MODULE_NONE, \
    VL53L1_TRACE_LEVEL_NONE, VL53L1_TRACE_FUNCTION_I2C, ##__VA_ARGS__)

// I2C Initialization
VL53L1_Error VL53L1_CommsInitialise(
    VL53L1_Dev_t *pdev,
    uint8_t       comms_type,
    uint16_t      comms_speed_khz)
{
    (void)comms_type;
    (void)comms_speed_khz;
    
    if (i2c_fd >= 0) {
        return VL53L1_ERROR_NONE; // Already initialized
    }
    
    i2c_fd = open(I2C_DEVICE, O_RDWR);
    if (i2c_fd < 0) {
        printf("Failed to open I2C device %s\n", I2C_DEVICE);
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    if (ioctl(i2c_fd, I2C_SLAVE, pdev->I2cDevAddr >> 1) < 0) {
        printf("Failed to set I2C address 0x%02X\n", pdev->I2cDevAddr >> 1);
        close(i2c_fd);
        i2c_fd = -1;
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    trace_i2c("VL53L1_CommsInitialise: I2C initialized at address 0x%02X\n", 
              pdev->I2cDevAddr >> 1);
    
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_CommsClose(VL53L1_Dev_t *pdev)
{
    (void)pdev;
    
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;
    }
    
    return VL53L1_ERROR_NONE;
}

// Write Multiple Bytes
VL53L1_Error VL53L1_WriteMulti(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint8_t      *pdata,
    uint32_t      count)
{
    (void)pdev;
    
    if (i2c_fd < 0) {
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    uint8_t *buffer = malloc(count + 2);
    if (!buffer) {
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    buffer[0] = (index >> 8) & 0xFF;
    buffer[1] = index & 0xFF;
    memcpy(&buffer[2], pdata, count);
    
    int result = write(i2c_fd, buffer, count + 2);
    free(buffer);
    
    if (result != (int)(count + 2)) {
        trace_i2c("VL53L1_WriteMulti failed\n");
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    return VL53L1_ERROR_NONE;
}

// Read Multiple Bytes
VL53L1_Error VL53L1_ReadMulti(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint8_t      *pdata,
    uint32_t      count)
{
    (void)pdev;
    
    if (i2c_fd < 0) {
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    uint8_t reg[2];
    reg[0] = (index >> 8) & 0xFF;
    reg[1] = index & 0xFF;
    
    if (write(i2c_fd, reg, 2) != 2) {
        trace_i2c("VL53L1_ReadMulti: Failed to write register address\n");
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    if (read(i2c_fd, pdata, count) != (int)count) {
        trace_i2c("VL53L1_ReadMulti: Failed to read data\n");
        return VL53L1_ERROR_CONTROL_INTERFACE;
    }
    
    return VL53L1_ERROR_NONE;
}

// Write Byte
VL53L1_Error VL53L1_WrByte(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint8_t       data)
{
    return VL53L1_WriteMulti(pdev, index, &data, 1);
}

// Write Word (16-bit)
VL53L1_Error VL53L1_WrWord(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint16_t      data)
{
    uint8_t buffer[2];
    buffer[0] = (data >> 8) & 0xFF;
    buffer[1] = data & 0xFF;
    return VL53L1_WriteMulti(pdev, index, buffer, 2);
}

// Write DWord (32-bit)
VL53L1_Error VL53L1_WrDWord(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint32_t      data)
{
    uint8_t buffer[4];
    buffer[0] = (data >> 24) & 0xFF;
    buffer[1] = (data >> 16) & 0xFF;
    buffer[2] = (data >> 8) & 0xFF;
    buffer[3] = data & 0xFF;
    return VL53L1_WriteMulti(pdev, index, buffer, 4);
}

// Read Byte
VL53L1_Error VL53L1_RdByte(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint8_t      *pdata)
{
    return VL53L1_ReadMulti(pdev, index, pdata, 1);
}

// Read Word (16-bit)
VL53L1_Error VL53L1_RdWord(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint16_t     *pdata)
{
    uint8_t buffer[2];
    VL53L1_Error status = VL53L1_ReadMulti(pdev, index, buffer, 2);
    *pdata = ((uint16_t)buffer[0] << 8) | buffer[1];
    return status;
}

// Read DWord (32-bit)
VL53L1_Error VL53L1_RdDWord(
    VL53L1_Dev_t *pdev,
    uint16_t      index,
    uint32_t     *pdata)
{
    uint8_t buffer[4];
    VL53L1_Error status = VL53L1_ReadMulti(pdev, index, buffer, 4);
    *pdata = ((uint32_t)buffer[0] << 24) | 
             ((uint32_t)buffer[1] << 16) | 
             ((uint32_t)buffer[2] << 8) | 
             buffer[3];
    return status;
}

// Wait Microseconds
VL53L1_Error VL53L1_WaitUs(
    VL53L1_Dev_t *pdev,
    int32_t       wait_us)
{
    (void)pdev;
    
    struct timespec ts;
    ts.tv_sec = wait_us / 1000000;
    ts.tv_nsec = (wait_us % 1000000) * 1000;
    nanosleep(&ts, NULL);
    
    return VL53L1_ERROR_NONE;
}

// Wait Milliseconds
VL53L1_Error VL53L1_WaitMs(
    VL53L1_Dev_t *pdev,
    int32_t       wait_ms)
{
    return VL53L1_WaitUs(pdev, wait_ms * 1000);
}

// Get Tick Count
VL53L1_Error VL53L1_GetTickCount(uint32_t *ptick_count_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *ptick_count_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    return VL53L1_ERROR_NONE;
}

// Wait for Value with Mask
VL53L1_Error VL53L1_WaitValueMaskEx(
    VL53L1_Dev_t *pdev,
    uint32_t      timeout_ms,
    uint16_t      index,
    uint8_t       value,
    uint8_t       mask,
    uint32_t      poll_delay_ms)
{
    uint32_t start_time_ms = 0;
    uint32_t current_time_ms = 0;
    uint8_t byte_value = 0;
    uint8_t found = 0;
    VL53L1_Error status = VL53L1_ERROR_NONE;
    
    VL53L1_GetTickCount(&start_time_ms);
    pdev->new_data_ready_poll_duration_ms = 0;
    
    while ((status == VL53L1_ERROR_NONE) &&
           (pdev->new_data_ready_poll_duration_ms < timeout_ms) &&
           (found == 0))
    {
        status = VL53L1_RdByte(pdev, index, &byte_value);
        
        if ((byte_value & mask) == value) {
            found = 1;
        }
        
        if (status == VL53L1_ERROR_NONE && found == 0 && poll_delay_ms > 0) {
            status = VL53L1_WaitMs(pdev, poll_delay_ms);
        }
        
        VL53L1_GetTickCount(&current_time_ms);
        pdev->new_data_ready_poll_duration_ms = current_time_ms - start_time_ms;
    }
    
    if (found == 0 && status == VL53L1_ERROR_NONE) {
        status = VL53L1_ERROR_TIME_OUT;
    }
    
    return status;
}

// GPIO Functions (stubs for Raspberry Pi - not needed for basic I2C operation)
VL53L1_Error VL53L1_GpioSetMode(uint8_t pin, uint8_t mode)
{
    (void)pin;
    (void)mode;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GpioSetValue(uint8_t pin, uint8_t value)
{
    (void)pin;
    (void)value;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GpioGetValue(uint8_t pin, uint8_t *pvalue)
{
    (void)pin;
    *pvalue = 0;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GpioXshutdown(uint8_t value)
{
    (void)value;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GpioCommsSelect(uint8_t value)
{
    (void)value;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GpioPowerEnable(uint8_t value)
{
    (void)value;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GpioInterruptEnable(void (*function)(void), uint8_t edge_type)
{
    (void)function;
    (void)edge_type;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GpioInterruptDisable(void)
{
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GetTimerFrequency(int32_t *ptimer_freq_hz)
{
    *ptimer_freq_hz = 0;
    return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_GetTimerValue(int32_t *ptimer_count)
{
    *ptimer_count = 0;
    return VL53L1_ERROR_NONE;
}
