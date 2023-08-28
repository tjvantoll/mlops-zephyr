/*
 * Copyright (c) 2023 Blues Inc.
 *
 * MIT License. Use of this source code is governed by licenses granted
 * by the copyright holder including that found in the LICENSE file.
 *
 * Author: Zachary J. Fields
 */

// Include Zephyr Headers
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

// Include Notecard note-c library
#include <note.h>

// Notecard node-c helper methods
#include "note_c_hooks.h"

// Uncomment the define below and replace com.your-company:your-product-name
// with your ProductUID.
#define PRODUCT_UID "com.blues.tvantoll:mlops"

#ifndef PRODUCT_UID
#define PRODUCT_UID ""
#pragma message "PRODUCT_UID is not defined in this example. Please ensure your Notecard has a product identifier set before running this example or define it in code here. More details at https://bit.ly/product-uid"
#endif

#define EI_CLASSIFIER_RAW_SAMPLE_COUNT           761
#define EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME      3
#define EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE       (EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
#define EI_CLASSIFIER_INTERVAL_MS                1.314060446780552
#define EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE + 256)

#define SLEEP_TIME_MS 1000 * 10

int main(void)
{
    int ret;

    printk("[INFO] main(): Initializing...\n");

    const struct device *const sensor = DEVICE_DT_GET_ANY(st_lis3dh);
    if (sensor == NULL) {
        printk("No device found!\n");
        return 0;
    }
    if (!device_is_ready(sensor)) {
        printk("Device %s is not ready\n", sensor->name);
        return 0;
    }

    // Initialize note-c hooks
    NoteSetUserAgent((char *)"note-zephyr");
    NoteSetFnDefault(malloc, free, platform_delay, platform_millis);
    NoteSetFnDebugOutput(note_log_print);
    NoteSetFnI2C(NOTE_I2C_ADDR_DEFAULT, NOTE_I2C_MAX_DEFAULT, note_i2c_reset,
                 note_i2c_transmit, note_i2c_receive);

    // Send a Notecard hub.set using note-c
    J *req = NoteNewRequest("hub.set");
    if (req)
    {
        JAddStringToObject(req, "product", PRODUCT_UID);
        JAddStringToObject(req, "mode", "continuous");
        JAddStringToObject(req, "sn", "tj-accelerometer");
        if (!NoteRequest(req))
        {
            printk("Failed to configure Notecard.\n");
            return -1;
        }
    }
    else
    {
        printk("Failed to allocate memory.\n");
        return -1;
    }

    // Application Loop
    printk("[INFO] main(): Entering loop 3...\n");

    while (1)
    {
        struct sensor_value accelerometerData[3];
        int16_t buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS] = {0};

        for (size_t ix = 0; ix < (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2); ix += 3)
        {
            // Determine the next tick (and then sleep later) (Arduino)
            // uint64_t next_tick_us = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);
            uint32_t next_tick_ms = NoteGetMs() + EI_CLASSIFIER_INTERVAL_MS;

            int rc = sensor_sample_fetch(sensor);
            if (rc != 0) {
                printk("Failed to fetch sensor data (error: %d)\n", rc);
                continue;
            }

            rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accelerometerData);
            if (rc != 0) {
                printk("Failed to get sensor data (error: %d)\n", rc);
                continue;
            }

            printk("Accelerometer reading: x=%d, y=%d, z=%d\n",
                sensor_value_to_micro(&accelerometerData[0]),
                sensor_value_to_micro(&accelerometerData[1]),
                sensor_value_to_micro(&accelerometerData[2])
            );
            buffer[ix] = sensor_value_to_micro(&accelerometerData[0]);
            buffer[ix + 1] = sensor_value_to_micro(&accelerometerData[1]);
            buffer[ix + 2] = sensor_value_to_micro(&accelerometerData[2]);

            // delayMicroseconds(next_tick_us - micros()); (Arduino)
            int32_t delay_ms = next_tick_ms - NoteGetMs();
            NoteDelayMs(delay_ms < 2 ? delay_ms : 2);
        }

        // testing
        // char buff[25] = "TJ VanToll";
        // NoteBinaryTransmit((uint8_t *) buff, 10, sizeof(buff), false);

        // send binary data to the Notecard (Arduino)
        // NoteBinaryTransmit(reinterpret_cast<uint8_t *>(buffer),
        NoteBinaryReset();
        NoteBinaryTransmit((uint8_t *)buffer,
            (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * 2),
            (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS * 2),
            0);

        J *req = NoteNewRequest("web.post");
        if (req)
        {
            JAddStringToObject(req, "route", "ingest");
            JAddBoolToObject(req, "binary", true);
            if (!NoteRequest(req))
            {
                printk("Failed to submit Note to Notecard.\n");
                ret = -1;
                break;
            }
        }
        else
        {
            printk("Failed to allocate memory.\n");
            ret = -1;
            break;
        }

        // Wait to iterate
        k_msleep(SLEEP_TIME_MS);
    }

    return ret;
}
