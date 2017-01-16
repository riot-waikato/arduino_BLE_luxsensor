# arduino_lux_fixed

This program enables transmission of light sensor readings via UART and then via Bluetooth if no acknowledgement is received.  Designed to work on a TinyDuino with the following modules:
- ASM2001 TinyDuino Processor Board
- ASD2101 USB TinyShield
- ASD2113 Bluetooth Low Energy TinyShield (Nordic)
- ASD2811 Ambient Light TinyShield

A proto board can be used to connect the TX/RX and GND pins to a separate module that will receive light readings via UART.  The USB connection also operates as a UART.

Any '+' character received via UART between readings is interpreted as a positive acknowledgement. A '-' character is interpreted as a negative acknowledgement, and this triggers a Bluetooth transmission if a connection to a peer is established, as does not receiving an acknowledgement by the time a new measurement is due.

## Dependencies

Heavily dependent on Nordic Semiconductor's ble-sdk-arduino library.

## Possible Future Work

- Follow recommendations for low power operation (page 45 of nRF8001 Product Specification)

## References:

https://github.com/NordicSemiconductor/ble-sdk-arduino
http://www.nordicsemi.com/eng/Products/Bluetooth-R-low-energy/nRF8001
Bluetooth Core Specification 4.2
