# tilt-relay

A [ESP32](http://esp32.net)-based [Tilt Hydrometer]([Tilt](https://tilthydrometer.com) Relay to [Brewfather](https://brewfather.app/)

```mermaid
graph LR
    A[Tilt<br/>Hydrometer] --->|Bluetooth LE<br/>iBeacon<br>gravity / temperature| B
    B[ESP32<br/>microcontroller] --->|WiFi<br/>http POST<br/>json| C[brewfather.app]
```

## Support

* [Tilt](https://tilthydrometer.com/products/copy-of-tilt-floating-wireless-hydrometer-and-thermometer-for-brewing)
* [Tilt Pro](https://tilthydrometer.com/products/tilt-pro-wireless-hydrometer-and-thermometer)
* BrewFather [Custom Stream](https://docs.brewfather.app/integrations/custom-stream)

## Why ?

The bluetooth signal from the Tilt Hydrometer is not strong enough to get
throught stainless fermenters (e.g. kegs) inside a metalic fermentation
chamber  (e.g. a freezer). This means the default brewfather integration
for the Tilts might now work properly without additional hardware.

Several solutions exists but the cheapest one is to use a microcontroller
inside the fermentation chamber (e.g. freezer) to listen to the bluetooth
signal and then relay it, using more powerful WiFi, to the brewfather app.

## How To Build

### Hardware

* A [ESP32](http://esp32.net) microcontroller

I used a [M5StickC](https://shop.m5stack.com/products/stick-c?variant=17203451265114).
It's main advantage is that it comes pre-installed inside it's own case.
It also has a small screen (not really needed for this project) and comes
with a short USB cable (more useful to develop than for a permanent
installation). Sadly it's now EOL and any supplies are likely be limited.

There is a newer [M5StickC Plus](https://shop.m5stack.com/collections/m5-controllers/products/m5stickc-plus-esp32-pico-mini-iot-development-kit)
model with a _larger_ screen. However it's pricier, does not come with
a USB cable and provide no additional/useful features for this project.

A cheaper option is the screen-less [Atom Lite](https://shop.m5stack.com/collections/m5-controllers/products/atom-lite-esp32-development-kit?variant=32259605200986)
but, from other projects, I found it's WiFi antenna is less powerful and
you might have issues (from inside a freezer) to connect it to your WiFi
network. YMMV.

### Software

* Install [VSCode](https://code.visualstudio.com)
* From VSCode install the [PlatformIO](https://platformio.org) extension
* Clone this repository
* Include your own `private.h` file with your WiFi credentials and your BrewFather [Custom Stream](https://docs.brewfather.app/integrations/custom-stream) URL

#### Example

```c
#ifndef __PRIVATE_H__
#define __PRIVATE_H__

#define DEFAULT_SSID        "wifi-ssid"
#define DEFAULT_PASSWORD    "wifi-password"
#define BREWFATHER_URL      "/stream?id=XXXXXXXXXX"
#endif
```

* Build the project
* Deploy to your ESP32 microcontroller

## See Also

* [Tilt Simulator](https://github.com/spouliot/tilt-sim)
