# tilt-relay

A ESP32 Tilt Hydrometer Relay to [Brewfather](https://brewfather.app/)

```mermaid
graph LR
    A[Tilt] -->|BLE: gravity temperature| B(ESP32)
    B -->|http: POST json| C[brewfather.app]
```

## Support

* [Tilt](https://tilthydrometer.com/products/copy-of-tilt-floating-wireless-hydrometer-and-thermometer-for-brewing)
* [Tilt Pro](https://tilthydrometer.com/products/tilt-pro-wireless-hydrometer-and-thermometer)
* BrewFather [Custom Stream](https://docs.brewfather.app/integrations/custom-stream)

## How To

* Provide your own `private.h` file with your WiFi credentials and your custom stream URL.

### Example

```c
#ifndef __PRIVATE_H__
#define __PRIVATE_H__

#define DEFAULT_SSID        "wifi-ssid"
#define DEFAULT_PASSWORD    "wifi-password"
#define BREWFATHER_URL      "/stream?id=XXXXXXXXXX"
#endif
```

* Build this project using [VSCode](https://code.visualstudio.com) and [PlatformIO](https://platformio.org).
* Deploy to an ESP32 microcontroller (and [M5StickC](https://shop.m5stack.com/products/stick-c?variant=17203451265114) was used)

## See Also

* [Tilt Simulator](https://github.com/spouliot/tilt-sim)
