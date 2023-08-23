/*
 * Copyright (c) 2023 Blues Inc.
 *
 * MIT License. Use of this source code is governed by licenses granted
 * by the copyright holder including that found in the LICENSE file.
 *
 * Author: Zachary J. Fields
 */

// Include Zephyr Headers
#include <zephyr/drivers/gpio.h>
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

#define SLEEP_TIME_MS 1000 * 30

int main(void)
{
    int ret;

    printk("[INFO] main(): Initializing...\n");

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
    printk("[INFO] main(): Entering loop...\n");
    while (1)
    {
        int16_t buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS] = {0};

        for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3)
        {
            // Determine the next tick (and then sleep later)
            // uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

            uint32_t next_tick = k_uptime_get_32() + (ix / 3) * EI_CLASSIFIER_INTERVAL_MS;

            // Eventually I want to make this read from the LIS3DH in Zephyr, but mocking
            // for now to just try and get COBS working
            // lis.read();
            // buffer[ix] = lis.x;
            // buffer[ix + 1] = lis.y;
            // buffer[ix + 2] = lis.z;
            buffer[ix] = 1;
            buffer[ix + 1] = 2;
            buffer[ix + 2] = 3;

            // delayMicroseconds(next_tick - micros());
            uint32_t current_time = k_uptime_get_32();
            if (current_time < next_tick)
            {
                k_sleep(K_MSEC(next_tick - current_time));
            }
        }

        // send binary data to the Notecard
        // NoteBinaryTransmit(reinterpret_cast<uint8_t *>(buffer),
        NoteBinaryTransmit((uint8_t *)buffer,
            (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * 2),
            (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE_COBS * 2),
            false);

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
