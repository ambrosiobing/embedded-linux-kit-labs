# Appendix

## Lab sequence and teardown

Work in this order if you are starting from bare boards.

1. P15 rails (do not skip).
2. P14 jig, then P16 and P17 sidecars.
3. P09 NanoPi bring-up.
4. P11 Explorer700 RTC, then take the HAT off.
5. P12 operator glass.
6. P05 MCC 118, P19 LCD tilt, P06 ToF kiosk, each tears the 40-pin down after acceptance.
7. P07 then P08, swapping the Nucleo shield, then P18 recorder, then P04 STWIN.
8. P01, P03, P02 cellular, then P13 failover.
9. P10 energy numbers on whatever DUT you still trust.
10. P20 last, so the fleet file reflects a known sitting.

> [!IMPORTANT]
> **Teardown**
>
> Power off before unseating a HAT. Discharge curiosity, not the modem PA: refit antennas before the next transmission. Put IKS4A1 and IKS5A1 back in separate bags, they look similar from across the bench.

## Reference AT and addresses

Three modules, three command sets. The full transcripts live in the labs that own them; this list is what to reach for at the bench.

- **SIM7600E-H, P01 and P13:** `AT`, `AT+CPIN?`, `AT+CSQ`, `AT+COPS?`, `AT+CEREG?`, `AT+CGPS=1`, `AT+CGPSINFO`
- **SIM7020E, P02 and P13:** `AT+CBAND`, `AT+CEREG?`, `AT+CPSMS`, `AT+CEDRXS`, `AT+CSOC`, `AT+CSOCON`, `AT+CSOSEND`
- **SIM7070G, P03:** `AT+CNMP=38`, `AT+CMNB=1`, `AT+CPSI?`, `AT+CGNSPWR`, `AT+CGNSINF`, `AT+SMCONF`, `AT+SMCONN`, `AT+SMPUB`

The ESP8266 AT set used in P17 is a fourth list and a much shorter one: `AT`, `AT+CWMODE=1`, `AT+CWJAP`, `AT+CIFSR`.

The 7-bit I2C addresses for every sensor in the bin are in the default I2C map in the front matter. Two of them are worth repeating here because they decide a lab: the ADXL345 answers at 0x53 with SDO floating, and on the IKS5A1 the ISM6HG256X is at 0x6A while the ISM330IS is at 0x6B.

## Raspberry Pi 40-pin header reference

The numbering below is the one used by every wiring table in this book. Pins 4, 7, 17, 27 and 28 are not named by any lab chapter; they follow the standard Raspberry Pi header and should be confirmed against the Raspberry Pi hardware documentation before they are used.

| Pin | Function | Pin | Function |
| --- | --- | --- | --- |
| 1 | 3V3 | 21 | GPIO9, MISO |
| 2 | 5V | 22 | GPIO25 |
| 3 | GPIO2, SDA1 | 23 | GPIO11, SCLK |
| 4 | 5V | 24 | GPIO8, CE0 |
| 5 | GPIO3, SCL1 | 25 | GND |
| 6 | GND | 26 | GPIO7, CE1 |
| 7 | GPIO4 | 27 | ID\_SD |
| 8 | GPIO14, TXD0 | 28 | ID\_SC |
| 9 | GND | 29 | GPIO5 |
| 10 | GPIO15, RXD0 | 30 | GND |
| 11 | GPIO17 | 31 | GPIO6 |
| 12 | GPIO18 | 32 | GPIO12 |
| 13 | GPIO27 | 33 | GPIO13 |
| 14 | GND | 34 | GND |
| 15 | GPIO22 | 35 | GPIO19 |
| 16 | GPIO23 | 36 | GPIO16 |
| 17 | 3V3 | 37 | GPIO26 |
| 18 | GPIO24 | 38 | GPIO20 |
| 19 | GPIO10, MOSI | 39 | GND |
| 20 | GND | 40 | GPIO21 |

*Table 1. The 40-pin header. Ground is on pins 6, 9, 14, 20, 25, 30, 34 and 39.*

Three groups of these pins carry most of the traffic in this book. Pins 1, 3, 5 and 6 are the I2C corner. P01 reaches it through the SIM7600E-H's full 40-pin pass-through, which leaves pins 3 and 5 untouched, and that has been checked. P19 cannot reach it at all: the 3.5 inch LCD (A) mates with the first 26 pins and carries no pass-through, so pins 1, 3, 5 and 6 sit under the panel. Since pin 1 and pin 17 are the only 3.3 V pins on the header and both are inside that covered range, a sensor on that host needs its supply from somewhere other than the header.

Pins 19, 21, 23, 24 and 26 are SPI0, which is why the MCC 118, the 3.5 inch LCD and the Explorer700 cannot share a host. The MCC 118 also claims GPIO12, GPIO13 and GPIO26 as board-address pins, and the ID EEPROM lives on pins 27 and 28 whatever is seated.

**The draft's semaphore triple was free all along, and this paragraph has been wrong about it twice.** It put the indicators on pins 13, 15 and 16, which on a bare Pi is fine and is what P15 uses. This paragraph then said the SIM7600E-H drives GPIO27, GPIO22 and GPIO23 as ring indicator, data terminal ready and clear to send, and separately that the SIM7020E claimed GPIO27 for its power key. **Both are withdrawn.** The vendor's schematic shows those three pins are header pass-through on the SIM7600E-H, with no net leaving the header and no pull on the board, and the vendor's wiki puts the SIM7020E's power key on GPIO4 like the SIM7070G's. No modem HAT in this bin claims any of the three.

P01, P02, P03 and P13 still use BCM 16, 20 and 21 on pins 36, 38 and 40, with grounds on pin 34 or 39, and the reason is now the only one left standing: one assignment that is safe across all four cellular labs is easier to hold than four, and these three are documented as free rather than merely assumed to be. The migration itself was unnecessary, and saying so is cheaper than leaving a reader to wonder why the book moved pins nothing was using. Confirm every one against the pinout of the board in front of you, because each is a fact about a HAT and not about the Raspberry Pi.

## NanoPi NEO Air 24-pin header

The NEO Air is a 24-pin board. It cannot accept any 40-pin HAT. Only the pins that the labs actually use are listed here; the rest of the header is outside the scope of this book.

| Pin | Signal | Used by |
| --- | --- | --- |
| 1 | SYS\_3V3 | ADXL345 VCC in P09, ESP8266 supply in P17. Do not put 5 V on this pin |
| 2 | VDD\_5V | Board supply, 5 V at 2 A. Never a sensor or module supply |
| 3 | I2C0\_SDA | ADXL345 SDA in P09 |
| 5 | I2C0\_SCL | ADXL345 SCL in P09 |
| 6 | GND | Common ground in P09 and P17 |
| 8 | UART1\_TX, PG6 | To the ESP8266 RX pin in P17 |
| 10 | UART1\_RX, PG7 | From the ESP8266 TX pin in P17 |

*Table 2. The 24-pin header pins used in this book. I2C0, UART1, SPI0 and GPIO are what this header offers, and no 40-pin HAT seats on it.*

| Pin | Signal | Note |
| --- | --- | --- |
| 1 | GND |  |
| 2 | 5V |  |
| 3 | TX | To the USB-TTL RX line |
| 4 | RX | From the USB-TTL TX line |

*Table 3. The separate 4-pin debug header, UART0. Renkforce USB-TTL at 3.3 V logic, 115200 8N1. This is the first login in P09 and the console of choice in P17.*

Confirm every one of these against the silkscreen before applying power. The NEO Air is a small board with two serial headers and a 3.3 V supply pin sitting next to a 5 V one.

## Which lab owns which part

One physics per lab, one exclusive part per lab. This table is the quickest way to see whether two labs can share a sitting: if they name the same part, they cannot.

| Exclusive part | Owner | Also appears in |
| --- | --- | --- |
| SIM7600E-H | P01 | P13 reuses the P01 attachment as the primary path |
| SIM7070G | P03 | Nowhere else. The same HAT cannot serve P02 |
| SIM7020E | P02 | P13 reuses the P02 attachment as the backup path |
| MCC 118 | P05 | Nowhere else. It is alone on the 40-pin header |
| RB-Explorer700 | P11 | Nowhere else. Do not move it onto a cellular Pi |
| 3.5 inch LCD (A) | P19 | P06 uses it as the ToF heat map, in a different sitting |
| Official DSI panel | P12 | P04 and P05 use the glass. It is not a HAT |
| STEVAL-STWINBX1 | P04 | Nowhere else. There is no second STWIN product lab |
| X-NUCLEO-IKS4A1 | P07 | P18 may stack it for the optional EKF session |
| X-NUCLEO-IKS5A1 | P08 | Nowhere else. Not interchangeable with IKS4A1 |
| X-NUCLEO-53L8A1 | P06 | Nowhere else. One Arduino shield on the Nucleo |
| SEN0032 ADXL345 | P01 | P09 on I2C0 and P19 as the tilt source |
| nRF PPK2 | P10 | Nowhere else. Never sources a Cat-4 PA |
| SBC-NodeMCU-ESP32 | P16 | P10 as the DUT, P14 as a provisioning target |
| SBC-ESP8266-PROG | P17 | P14 as a provisioning target |
| NanoPi NEO Air | P09 | P17 as the air-gap host, P20 as the fourth host |

*Table 4. Exclusive parts and the labs that own them. The Nucleo-H7A3ZI-Q is shared by P06, P07, P08 and P18, one shield at a time.*

## Glossary

- **AP6212:** The Wi-Fi and Bluetooth part on the NanoPi NEO Air. It shares one radio between both functions, and P17 switches it off for the whole session.
- **APN:** Access point name. The operator string a cellular module needs before it can carry data. P02 takes it from the SIM sleeve.
- **AT command:** The text control language of a cellular or Wi-Fi module. Sent on a serial port, one line at a time, with an answer read before the next line.
- **Cat-4:** The LTE category of the SIM7600E-H. It carries a default IPv4 route in P01 and is the only such route in the book.
- **Cat-M:** Also eMTC. The low-power LTE mode locked in P03 with `AT+CMNB=1`. A module that falls back to GPRS fails that lab.
- **CDC-ACM:** The USB communications device class that presents a serial port to Linux as `/dev/ttyACM*`. It exists only after the firmware declares it, which is the correction P04 makes to the draft.
- **CEREG:** The AT query that reports EPS network registration. It drives the green LED in P01 and P02.
- **CoAP:** A compact request and response protocol over UDP, used in P02 because an NB-IoT node cannot afford a session-heavy stack.
- **daqhats:** The MCC userspace library that talks to the MCC 118 over SPI. It is the supported path in P05, and there is no mainline IIO driver for that board.
- **dead-band:** The window around 1 g in P01 inside which no event is published. It turns a sample stream into an event stream.
- **DSI:** The display serial interface the official Raspberry Pi touchscreen uses. It does not consume the 40-pin header, so a DSI panel may coexist with a HAT.
- **DTO:** Device tree overlay. The mechanism that binds a board-level device such as the DS3231 at boot. P11 is the overlay lab because the chips are there.
- **eDRX:** Extended discontinuous reception. A negotiated listening schedule that lets an NB-IoT module sleep between paging windows. Requested in P02 with `AT+CEDRXS`.
- **eMMC:** The soldered flash on the NanoPi NEO Air. P09 discusses its wear and keeps append-heavy logs off the rootfs.
- **F2FS:** A flash-friendly filesystem. In P09 it is an option for a data partition you created yourself, never for the rootfs you just booted.
- **GNSS:** Satellite positioning. P03 stamps its payloads with a GNSS time and powers the receiver down before transmitting.
- **HAT:** A board that seats on the Raspberry Pi 40-pin header. Six of them exist in this bin and exactly one may be fitted at a time.
- **hwclock:** The Linux tool that reads and writes a hardware real-time clock. In P11 the DS3231 on the Explorer700 is that clock, and the lab proves it by pulling the power.
- **I2C:** The two-wire bus on pins 3 and 5 of the Pi header and on pins 3 and 5 of the NanoPi header. Every address used in this book is in the default I2C map.
- **ID EEPROM:** The small memory on pins 27 and 28 that identifies a seated HAT. P20 reports it honestly, including reporting none.
- **IIO:** The Linux industrial I/O subsystem. Named here mainly to record that the MCC 118 does not use it.
- **isolcpus:** The kernel parameter that keeps a CPU free of the general scheduler. P04 uses it with `taskset` so the FFT thread is not migrated.
- **ISPU:** The intelligent sensor processing unit inside the ISM330IS on the IKS5A1. A programmable core in the sensor, not a machine learning core.
- **MLC:** Machine learning core. It lives in the ISM330DHCX on the STWIN.box, and the audit rejects any lab that puts it on the IKS5A1.
- **MQTT:** The publish and subscribe protocol used by P01, P03, P12, P13 and P16. In P16 the topic is the entire contract between the Pi and the ESP32.
- **NB-IoT:** The narrowband cellular mode of the SIM7020E. It is not a phone network, and P02 measures a duty cycle rather than a ping.
- **PSM:** Power saving mode. The state in which a cellular module stays registered but stops listening. Requested in P02 with `AT+CPSMS`.
- **QMI:** Qualcomm MSM interface. With `qmi_wwan` it is the supported alternative to RNDIS for the P01 user plane, and it is never run beside PPP.
- **Qvar:** The electric charge variation sensing channel on the IKS4A1. P07 enables it only after the IMU stream is clean.
- **RNDIS:** A USB networking class that presents the modem as an Ethernet-like interface, typically `usb0`. The first thing P01 tries.
- **source-measure:** Supplying a device and measuring its current on the same terminals. The PPK2 does this in P10, which is why the DUT USB cable must come out.
- **SPI:** The serial bus on pins 19, 21, 23, 24 and 26. The MCC 118, the 3.5 inch LCD and the Explorer700 all want it, which is one more reason they cannot share a Pi.
- **ToF:** Time of flight ranging. The VL53L8CX in P06 returns an 8 by 8 grid of millimetre values, streamed to Linux over USB-CDC.
- **UART:** An asynchronous serial port. Three of them appear in P17 alone: the NanoPi UART1 modem path, the UART0 debug console and the USB-UART on the ESP8266-PROG.
- **USB-CDC:** The USB serial path from a microcontroller to a Linux host. It is how the Nucleo and the STWIN.box speak in P04, P06, P07, P08 and P18.

---

[Previous](20-four-host-fleet-health.md) &nbsp;&nbsp;|&nbsp;&nbsp; [Contents](../README.md)
