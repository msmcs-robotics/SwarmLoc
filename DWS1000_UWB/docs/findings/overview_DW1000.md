> use the UWB DWS1000 for accuracy of Time of Flight (ToF) to within 5-10 cm
> This setup is using the Qorvo PCL298336 v1.3 shield, which is an Arduino shield format for the DWM1000

**Difference between M and S model**

DWM1000 – UWB Transceiver Module
DWS1000 – Arduino-Compatible Development Shield

[Buy DWS1000 From DigiKey](https://www.digikey.com/en/products/detail/qorvo/DWS1000/12088519)

Tech Specs
- Support 6 channels on Frequency Range: 3.5 GHz to 6.5 GHz
- UWB Compliance: IEEE 802.15.4-2011
- Supply Voltage: 3.3V
- SPI interface to host processor

[DW1000 Library](https://github.com/thotro/arduino-dw1000)

**Pinout**

| DWS1000 Pin |	Arduino Uno |
|---|---|
| MOSI | D11 |
| MISO | D12 |
| SCK | D13 |
| CS | D10 |
| RST | D9 |
| IRQ | D2 |
| GND | GND |
| VCC | 3.3V |


The Qorvo PCL298336 module shield (which appears to be the DWM3000EVB Arduino shield) has a different pinout than the standard DWS1000 breakout boards. This is an Arduino shield format, so it plugs directly into the Arduino headers.

Based on the search results, here's what you need to know:

## Key Differences for PCL298336/DWM3000EVB Shield:

1. **Shield Format**: This Arduino shield can be used with the customer's choice of microcontroller. The Arduino shield form factor is supported by many MCU vendors
   1. [See here](https://www.qorvo.com/products/p/DWM3000EVB)

2. **Direct Connection**: The SPI connections are directly from the pins to the module. The extra parts look to be LEDs on the GPIO lines and a capacitor on the power input

3. **Different Chip**: This uses the DWM3000 (newer) instead of DWS1000, which requires different libraries

## Updated Setup for PCL298336/DWM3000EVB:

### Hardware Connection:
- **No wiring needed!** The shield plugs directly into Arduino Uno headers
- The shield handles all the pin connections automatically

### Software Changes:
You'll need to use DWM3000 libraries instead of DWS1000 libraries:

1. **GitHub Library**: There's a port of Qorvo/Decawave's DWM3000 Module on the DWS3000 Arduino Shield [available on GitHub](https://github.com/foldedtoad/dwm3000)

2. **Library Installation**:
   - Go to: https://github.com/foldedtoad/dwm3000
   - Download and install this library instead of the DW1000 library

### Standard DWM3000EVB Shield Pinout:
The shield typically uses these Arduino pins:
- **SPI**: Standard Arduino SPI pins (11, 12, 13)
- **CS**: Pin 10 (or pin 7, varies by version)
- **IRQ**: Pin 2
- **RST**: Pin 9
- **Power**: 3.3V and GND from Arduino headers

### Important Notes:
- **Library Compatibility**: Some users have reported difficulty finding working libraries and code examples for the DWM3000 Arduino Shield
- **Version Differences**: Make sure you're using the correct version (v1.3) specific documentation
- **Power Requirements**: The shield is designed to work with Arduino's power supply

Would you like me to help you find the specific pinout documentation for your v1.3 shield, or help you set up the DWM3000 library instead of the DWS1000 code I provided earlier?