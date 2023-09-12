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

// Include C standard library headers
#include <math.h>
#include <string.h>

// Include Notecard note-c library
#include <note.h>

// Notecard node-c helper methods
#include "note_c_hooks.h"

// Uncomment the define below and replace com.your-company:your-product-name
// with your ProductUID.
// #define PRODUCT_UID "com.your-company:your-product-name"

#ifndef PRODUCT_UID
#define PRODUCT_UID ""
#pragma message "PRODUCT_UID is not defined in this example. Please ensure your Notecard has a product identifier set before running this example or define it in code here. More details at https://bit.ly/product-uid"
#endif

#define EI_CLASSIFIER_RAW_SAMPLE_COUNT           761
#define EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME      3
#define EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE       (EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
#define EI_CLASSIFIER_INTERVAL_MS                1.314060446780552
#define EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE + 256)

#define SLEEP_TIME_MS 1000 * 60

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

    // Perform a hub.set request on the Notecard using note-c
    {
        J *req = NoteNewRequest("hub.set");
        if (req)
        {
            JAddStringToObject(req, "product", PRODUCT_UID);
            JAddStringToObject(req, "mode", "continuous");
            JAddStringToObject(req, "sn", "your-device-name");
            if (!NoteRequest(req))
            {
                printk("Failed to configure Notecard with hub.set\n");
                return -1;
            }
        }
        else
        {
            printk("Failed to allocate memory.\n");
            return -1;
        }
    }

    // Perform a card.dfu request on the Notecard using note-c
    {
        J *req = NoteNewRequest("card.dfu");
        if (req)
        {
            JAddStringToObject(req, "name", "stm32");
            JAddBoolToObject(req, "on", true);
            if (!NoteRequest(req))
            {
                printk("Failed to configure with card.dfu\n");
                return -1;
            }
        }
        else
        {
            printk("Failed to allocate memory.\n");
            return -1;
        }
    }

    // Application Loop
    printk("[INFO] main(): Entering loop...\n");

    float *buffer = (float *)malloc(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS
        * sizeof(float));
    if (!buffer){
        printk("Failed to allocate memory for classifier buffer.\n");
        return -1;
    }
    memset(buffer, 0, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS
        * sizeof(float));

    while (1)
    {
        struct sensor_value accelerometerData[3];

        for (size_t ix = 0; ix < (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2);
            ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
        {
            // Determine the next tick (and then sleep later)
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

            float x = sensor_value_to_float(&accelerometerData[0]);
            float y = sensor_value_to_float(&accelerometerData[1]);
            float z = sensor_value_to_float(&accelerometerData[2]);
            printk("x: %f, y: %f, z: %f\n", x, y, z);

            buffer[ix] = x;
            buffer[ix + 1] = y;
            buffer[ix + 2] = z;

            int32_t delay_ms = next_tick_ms - NoteGetMs();
            NoteDelayMs(delay_ms < 2 ? delay_ms : 2);
        }

        // Send binary data to the Notecard
        NoteBinaryStoreReset();
        NoteBinaryStoreTransmit((uint8_t *)buffer,
            (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(float)),
            (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS * sizeof(float)),
            0);

        J *req = NoteNewRequest("web.post");
        if (req)
        {
            JAddStringToObject(req, "route", "ingest");
            JAddStringToObject(req, "content", "binary/octet-stream");
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

    free(buffer);
    return ret;
}
