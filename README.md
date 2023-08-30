# Zephyr MLOps Sample

This repository contains a Zephyr application that uses the [Notecard](https://blues.io/products/notecard/), [Swan](https://blues.io/products/swan/), and an [LIS3DH acceleromter](https://www.adafruit.com/product/2809) to stream acceleromter data to [Edge Impulse](https://edgeimpulse.com/) through a [proxy route](https://dev.blues.io/api-reference/notecard-api/web-requests/).

## Hardware

This project’s firmware expects to run on a Blues Swan, connected to a Notecarrier-F that also has a connected Notecard and LIS3DH acceleromter.

![The necessary hardware setup](https://user-images.githubusercontent.com/544280/264364032-ad567dc6-b545-4c7e-8936-c471b1cfc26b.jpg)

You can get all the necessary hardware by purchasing a [Blues Starter Kit](https://shop.blues.io/collections/blues-starter-kits), and [LIS3DH](https://www.adafruit.com/product/2809). Once you have the hardware, refer to the [Blues quickstart](https://dev.blues.io/quickstart/blues-quickstart/) for details on how to connect everything.

## Running

This repository is built on top of the [Notecard’s Zephyr SDK](https://dev.blues.io/tools-and-sdks/firmware-libraries/zephyr-sdk/), and you can refer to its documentation for details on how to set up, build, and clone this project.

* [Requirements](https://dev.blues.io/tools-and-sdks/firmware-libraries/zephyr-sdk/#requirements)
* [Setup](https://dev.blues.io/tools-and-sdks/firmware-libraries/zephyr-sdk/#getting-set-up)
* [Building and Flashing](https://dev.blues.io/tools-and-sdks/firmware-libraries/zephyr-sdk/#building-and-running)

Once you have everything set up, find and uncomment the line in `main.c` that defines the project’s `PRODUCT_UID`, and replace its value with your [appropriate Notehub ProductUID](https://dev.blues.io/notehub/notehub-walkthrough/#finding-a-productuid).

```c
#define PRODUCT_UID "your value here"
```

Finally, you’ll need to ensure your Notehub route has a proxy route configured named “ingest” (or change the name of the `"route"` in `main.c`). This project will stream acceleromter readings from the LIS3DH to this route.

See the [Accelerometer Data Ingest](https://github.com/tjvantoll/accelerometer-data-ingest) for an example implementation of a server that can accept these readings and send them to Edge Impulse.

