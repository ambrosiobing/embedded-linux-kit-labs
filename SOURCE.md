# Source content (transcribed from the attached cookbook)

This file is the authority for every fact in this volume. It is a transcription of
`embedded_linux_top20.pdf` / `.html` (the audited kit cookbook, 39 pages, 2026).
Do not contradict it. Do not add hardware that is not here. Where the source is
silent on a detail you need for a diagram, draw only what the source states and
say "confirm on the silkscreen" in the text.

## Non-negotiable lab rules (front matter)

1. **No new parts.** HAT antennas are in-scope. A data SIM is assumed only for P01 to P03 and P13. Without a SIM those labs stop at AT bring-up, and that is still a pass.
2. **HAT exclusivity.** SIM7600E-H, SIM7070G, SIM7020E, MCC 118, JOY-iT Explorer700 and Waveshare 3.5 inch LCD (A) each own the 40-pin header. Fit one. The official DSI touchscreen does not consume the header.
3. **NanoPi is 24-pin.** It cannot accept any 40-pin HAT. Do not seat SIM7020, SIM7070, SIM7600, MCC 118, Explorer700 or the 3.5 inch LCD on the NEO Air.
4. **3.3 V GPIO.** Pi, NanoPi and Nucleo I/O are 3.3 V. POW-BB is a labelled rail splitter.
5. **One physics per lab.** IKS4A1, IKS5A1, STWIN, ADXL345, VL53L8CX and MCC 118 are six different instruments, never interchangeable.
6. **Linux owns the system.** Nucleo, STWIN, ESP32 and ESP8266 speak USB-CDC, UART or I2C to a host that owns storage, time, networking and the UI. A bare-metal sketch is allowed only when a Linux host still records the stream (P18).

Build one lab at a time. Tear the 40-pin HAT off before the next lab.

This edition starts from two sources: the kit-only 20-lab split, and a 198-page "advanced architectural blueprints" draft. The draft had the right ambition (QMI instead of PPP, PSM/eDRX, sensor-hub modes, PPK2 on the modem rail, store-and-forward). It also contained pairings that cannot be built from this bin. This book keeps the ambition and throws out the impossible wiring.

Hosts run Raspberry Pi OS Bookworm; the NanoPi runs FriendlyElec Ubuntu.

## Audit verdict table

| Lab | Primary capability | Host + exclusive part | Status |
|-----|--------------------|-----------------------|--------|
| P01 | LTE Cat-4 default route + shock events | Pi 4 + SIM7600 + ADXL | Buildable |
| P02 | NB-IoT PSM/eDRX CoAP | Pi 3 + SIM7020 | Buildable (SIM required for RF) |
| P03 | Cat-M MQTT + GNSS stamp | Pi 3B+ + SIM7070 | Buildable; lock AT+CMNB=1 |
| P04 | 6 kHz vibration + ultrasound CDC | Pi 4 + STWIN + DSI | Buildable; firmware chooses CDC |
| P05 | 10 V 100 kS/s analog | Pi 4 + MCC 118 | Buildable via daqhats, not IIO |
| P06 | 8x8 ToF occupancy kiosk | Pi 3 + LCD + Nucleo + 53L8 | Buildable; two hosts |
| P07 | Consumer 9-DoF + RH + Qvar | Nucleo + IKS4A1 + Pi CDC | Buildable |
| P08 | Industrial high-g + 4060 hPa | Nucleo + IKS5A1 + Pi CDC | Buildable; not ISM330DHCX |
| P09 | Allwinner headless I2C hub | NanoPi + ADXL + UART0 | Buildable; 24-pin only |
| P10 | Source-measure energy figures | Pi 4 + PPK2 + ESP32 DUT | Buildable; isolate DUT USB |
| P11 | RTC / OLED / IR / DTO console | Pi 3B+ + Explorer700 | Buildable |
| P12 | Operator glass + Mosquitto | Pi 4 + official DSI + keyboard | Buildable; no 40-pin HAT |
| P13 | Signalling failover 4G to NB | Pi 4+7600 and Pi 3+7020 | Buildable; two hosts |
| P14 | Serial provisioning jig | Pi 3 + USB-TTL + ESP32/8266 | Buildable |
| P15 | Mixed-voltage GPIO discipline | POW-BB + LEDs + any host | Buildable |
| P16 | Wi-Fi/BLE sidecar | Pi 4 + ESP32 | Buildable |
| P17 | AT-command air-gap modem | NanoPi + ESP8266, onboard Wi-Fi off | Buildable |
| P18 | High-rate USB-CDC recorder | Pi 4 + bare Nucleo (EKF optional) | Buildable; Linux still records |
| P19 | Offline SPI-LCD tilt meter | Pi 3 + 3.5 inch LCD + ADXL | Buildable; LCD owns header |
| P20 | Four-host fleet health | All Linux boards + SSH | Buildable |

## Do not build these pairings (errors found in the 198-page blueprint)

- **NanoPi + any 40-pin HAT** (draft P4, P12). NEO Air is a 24-pin header. SIM7020E does not seat. NB-IoT moves to Pi 3 (our P02).
- **Explorer700 + SIM7600 on one Pi** (draft P18). Two 40-pin HATs. Split across hosts, or drop one HAT.
- **IKS5A1 hosting an ISM330DHCX MLC** (draft P13). That IMU is on the STWIN.box. IKS5A1 has ISM330IS (ISPU) and ISM6HG256X. Our P08 uses the chips that are actually on IKS5A1.
- **MCC 118 as a mainline IIO + DMABUF + io_uring device** (draft P2, P17). The supported path is SPI plus the daqhats userspace library. There is no IIO_BUFFER_DMABUF_ATTACH_IOCTL for this HAT.
- **Intel MKL on a Pi 4** (draft P1). Use NumPy, FFTW or OpenBLAS.
- **SIM7070 stacked on the Nucleo as a HAT** (draft P3). The modem is a Pi HAT. The Nucleo talks UART to a Pi that owns the modem, or to breakout pins with flying leads, never as a stacked Arduino HAT.
- **Yocto-from-scratch as a week-one lab** (draft P12). A valid stretch goal on a separate x86 build machine. The kit lab is FriendlyElec Ubuntu on eMMC (our P09).
- **"VCP" as a property of USB-C** (draft P1). USB-C is the connector. CDC-ACM appears only after the STWIN firmware declares it.
- **stsw-img042 V4L2 ToF node on Raspberry Pi OS** (draft P8). ST's Linux ToF stack is not a drop-in Bookworm camera device. Stream 64 ranges over the Nucleo USB-CDC instead (our P06).
- **Draft P1 and P20 both owning STWIN vibration uplink.** Keep P04 as USB raw plus FFT and do not add a second STWIN product lab.

Ideas from the draft that are kept: QMI/RNDIS instead of PPP for Cat-4; PSM/eDRX on NB-IoT; GNSS powered down during LTE-M transmission; PPK2 in the DUT power rail; DS3231 as hwclock; Explorer700 overlays; anomaly-only uplink as a software policy on P04, not a second project; isolcpus for the FFT thread.

## Inventory

| Item | PN | Role |
|------|----|------|
| RPi 4 / 3B+ / 3 | RPi | Bookworm hosts |
| NanoPi NEO Air | FriendlyElec | H3, 512 MB, 8 GB eMMC, Wi-Fi/BT, **24-pin** |
| NUCLEO-H7A3ZI-Q | ST | STM32H7A3 USB-CDC sensor brain |
| X-NUCLEO-IKS4A1 | ST | LSM6DSO16IS, LSM6DSV16X, LIS2DUXS12, LIS2MDL, LPS22DF, SHT40, STTS22H, Qvar |
| X-NUCLEO-IKS5A1 | ST | ISM6HG256X, ISM330IS, IIS2DULPX, IIS2MDC, ILPS22QS |
| X-NUCLEO-53L8A1 | ST | VL53L8CX 8x8 ToF |
| STEVAL-STWINBX1 | ST | IIS3DWB 6 kHz, ISM330DHCX + MLC, IMP34DT05, IMP23ABSU |
| nRF PPK2 | Nordic | 100 kS/s current source-measure |
| SEN0032 ADXL345 | DFRobot | I2C 0x53 3-axis, INT1 available |
| SIM7600E-H 4G HAT | Waveshare | LTE Cat-4 + GNSS, USB RNDIS/QMI |
| SIM7070G HAT | Waveshare | Cat-M / NB-IoT / GPRS + GNSS |
| SIM7020E HAT | Waveshare | NB-IoT only |
| MCC 118 | Digilent/MCC | 8 ch, 10 V, 12-bit, 100 kS/s, SPI |
| 3.5 inch RPi LCD (A) | Waveshare | 480x320 SPI + resistive touch |
| RB-Explorer700 | JOY-iT | OLED, DS3231, BMP280, PCF8591, PCF8574, joystick, IR, buzzer |
| SBC-NodeMCU-ESP32 | JOY-iT | Wi-Fi + BLE coprocessor |
| SBC-ESP8266-PROG | JOY-iT | AT-modem sidecar |
| USB/TTL cable | Renkforce | 3.3 V UART |
| SBC-POW-BB + LEDs | JOY-iT | Rails and semaphores |
| Touchscreen + keyboard | RPi | Operator HMI (DSI, not a HAT) |

## Default I2C map (7-bit)

| Device | Board | Address |
|--------|-------|---------|
| ADXL345 | SEN0032 | 0x53 |
| LIS2MDL / IIS2MDC | IKS4 / IKS5 / STWIN | 0x1E |
| SHT40AD1B | IKS4A1 | 0x44 |
| STTS22H | IKS4 / STWIN | 0x38 |
| LPS22DF | IKS4A1 | 0x5C |
| LSM6DSO16IS / LSM6DSV16X | IKS4A1 | 0x6A |
| ISM6HG256X | IKS5A1 | 0x6A |
| ISM330IS | IKS5A1 | 0x6B |
| ILPS22QS | IKS5 / STWIN | 0x5C |
| IIS2DULPX | IKS5A1 | 0x19 |
| DS3231 | Explorer700 | 0x68 |
| BMP280 | Explorer700 | 0x76 or 0x77 |
| PCF8591 | Explorer700 | 0x48 |
| PCF8574 | Explorer700 | 0x20 to 0x27 |
| VL53L8CX | 53L8A1 I2C mode | 0x29 |

## HAT XOR and coexistence

```
40-pin XOR (one per Pi):  SIM7600 | SIM7070 | SIM7020 | MCC118 | Explorer700 | 3.5" LCD
May coexist with a HAT:   official DSI touchscreen, USB gadgets
USB-host gadgets:         STWIN, Nucleo, PPK2, ESP32, ESP8266, Renkforce TTL
NanoPi 24-pin only:       I2C0 / UART1 / SPI0 / GPIO --- no 40-pin HAT
Arduino-stack XOR:        IKS4A1 | IKS5A1 | 53L8A1  (one shield on the Nucleo)
```

---

# P01 - LTE Cat-4 motion-triggered gateway

**Unique:** owns the only Cat-4 default route in the book, plus ADXL345 event compression. Cat-M and NB-IoT are P03 and P02.

**Blueprint corrections:** draft P5 is the same radio used as a NAT router; draft P1 used Intel MKL. We use QMI/RNDIS for the WAN and NumPy for nothing heavier than a magnitude check. Do not also put GNSS tracking here, that is P03.

**Intent:** Pi 4 gets a default IPv4 route via SIM7600E-H USB (RNDIS or QMI-WWAN). ADXL345 publishes a shock event only when |a| minus 1 g exceeds a dead-band. LEDs show registered / WAN-up / event.

**Kit:** Pi 4, SIM7600E-H + LTE/GNSS antennas + micro-SIM, SEN0032 ADXL345, R/Y/G LEDs on BCM 27/22/23, 3 A USB-C PSU, keyboard.

**Wiring:**
```
ADXL345                 Pi 40-pin (HAT pass-through pins 1/3/5/6 still available)
VCC ------------------- pin 1  3V3      NEVER 5V
GND ------------------- pin 6  GND
SDA ------------------- pin 3  GPIO2
SCL ------------------- pin 5  GPIO3
SDO floating => 0x53
INT1 optional --------- BCM17  (not required for the magnitude poll)

SIM7600 jumpers:
  VCCIO = 3V3
  PWR   = 3V3   (auto-on). Use PWR=D6 only if you want GPIO power-key.
  UART  = B if you need ttyAMA0; prefer the HAT USB cable for AT + data.
USB cable: HAT USB-A/micro -> Pi USB-A  (user plane + AT channels)
Antennas: LTE main + GNSS. No antenna = no attach, and can stress the PA.
LED GRN BCM27 = CEREG registered
LED YEL BCM22 = default route on usb0/wwan0
LED RED BCM23 = last shock event
```

**Steps:**
1. Flash Raspberry Pi OS Bookworm 64-bit. Enable I2C (raspi-config, Interface). Use a 3 A PSU.
2. Seat the HAT, fit both antennas, insert the SIM, connect the HAT USB cable before applying power.
3. Identify the modem and the accelerometer:
```
lsusb | grep -iE "sim|1e0e|1e0e"
dmesg | tail -n 40
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
sudo apt-get update
sudo apt-get install -y minicom i2c-tools mosquitto-clients python3-smbus python3-rpi.gpio
sudo i2cdetect -y 1          # must show 53
```
4. Open the AT port (commonly /dev/ttyUSB2 on SIM7600; if minicom shows nothing, try USB3 then USB1):
```
sudo minicom -D /dev/ttyUSB2 -b 115200
AT
ATE1
AT+CPIN?
AT+CSQ
AT+COPS?
AT+CEREG?
AT+CGDCONT?
```
5. Bring up the WAN. Prefer RNDIS/QMI over PPP:
```
# RNDIS path (usb0 typically appears when the HAT USB cable is connected)
ip link
sudo dhcpcd usb0 || sudo dhclient usb0
ip route
# If usb0 did not appear, load qmi_wwan / option and use ModemManager:
sudo apt-get install -y modemmanager libqmi-utils
mmcli -L
# Do not run PPP in parallel with QMI/RNDIS.
```
6. Confirm the default route is the modem, not wlan0, then lock it:
```
ip route | grep default
# optional: metric wlan0 higher so LTE wins
sudo ip route del default dev wlan0
```
7. Run the event publisher (save as /home/pi/p01_shock.py):
```
import time, math, json, subprocess, smbus
bus = smbus.SMBus(1); A = 0x53
bus.write_byte_data(A, 0x2D, 0x08)   # measure
bus.write_byte_data(A, 0x31, 0x0B)   # full-res +/-16g
def g(lo, hi):
    v = (hi << 8) | lo
    if v & 0x8000: v -= 65536
    return v / 256.0
cool = 0
while True:
    b = bus.read_i2c_block_data(A, 0x32, 6)
    ax, ay, az = g(b[0], b[1]), g(b[2], b[3]), g(b[4], b[5])
    mag = math.sqrt(ax*ax + ay*ay + az*az)
    if abs(mag - 1.0) > 0.35 and time.time() > cool:
        payload = json.dumps({"mag": round(mag, 3), "ax": round(ax, 3)})
        subprocess.run(["mosquitto_pub", "-h", "test.mosquitto.org",
                        "-t", "lab/p01/shock", "-m", payload], check=False)
        cool = time.time() + 2
    time.sleep(0.05)
```

**Acceptance test:** fist on the table produces one MQTT JSON and the red LED. `ping -I usb0 8.8.8.8` works (or the operator DNS). Outdoors, AT+CGPS=1 then AT+CGPSINFO returns non-blank fields, a side check, not this lab's product.

**Practices:** user plane on USB, AT on a ttyUSB. Token-bucket the publisher (the 2 s cool-off). Log AT+CSQ every 60 s. Do not seat MCC 118 or the LCD at the same time.

---

# P02 - NB-IoT PSM/eDRX CoAP field node

**Unique:** owns NB-IoT and the PSM/eDRX duty cycle. No default IPv4 WAN, no GNSS, no MQTT. The opposite of P01.

**Blueprint corrections:** draft P4 seated the SIM7020E on the NanoPi. That header is 24-pin. NB-IoT lives on the Pi 3 in this book.

**Intent:** SIM7020E on a Pi 3 as a power-saving NB-IoT endpoint. Every 10 minutes the node wakes, sends an approximately 20-byte CoAP/UDP datagram, and returns to idle. Acceptance is the duty cycle, not a continuous ping.

**Kit:** Pi 3, SIM7020E HAT + NB antenna + NB-IoT SIM, green LED BCM27.

**Wiring:**
```
HAT seated on Pi 3. UART jumper = Waveshare "B" (Pi controls modem).
Disable Bluetooth to free PL011 ttyAMA0:
  /boot/firmware/config.txt   dtoverlay=disable-bt
  raspi-config -> Serial: login=OFF, hardware=ON
LED GRN BCM27 = CEREG registered.
Antenna on the NB port. No 4G antenna swap, the matching is different.
```

**Steps:**
1.
```
sudo systemctl disable --now hciuart
sudo apt-get install -y minicom python3-serial
sudo minicom -D /dev/ttyAMA0 -b 115200
```
2. Identity and attach (band is operator-specific; EU commonly B8/B20):
```
AT
AT+CPIN?
AT+CBAND=20
AT+COPS=0
AT+CEREG?
AT+CGATT?
AT+CSQ
AT+CGDCONT?
```
3. Enable PSM. Encoded TAU / active-time bitmaps are operator-checked; the values below request a long TAU and a short active window. Confirm with the SIM sheet before leaving this running unattended:
```
AT+CPSMS=1,,"00100011","00000101"
AT+CEDRXS=1,5,"0010"
AT+CPSMS?
```
4. Open a UDP socket toward a CoAP endpoint you control (or coap.me only as a lab echo, many NB APNs cannot reach the public Internet):
```
AT+CSOC=1,2,1
AT+CSOCON=0,5683,"172.16.0.1"
AT+CSOSEND=0,8,"50494E47"
```
5. Wrap the send in a 10-minute cron or a Python loop that power-cycles the radio with AT+CFUN=0 / AT+CFUN=1 if the operator does not honour PSM from this HAT.

**Acceptance test:** AT+CSQ and AT+CEREG? show a cell. One datagram per 10 minutes appears on the server. A continuous ping is a fail.

**Practices:** NB-IoT is not a phone network. Do not expect test.mosquitto.org. Use the APN printed on the SIM sleeve. Measure watts in P10, not here.

---

# P03 - Cat-M MQTT node with GNSS stamp

**Unique:** owns Cat-M and the GNSS-timestamp-in-payload rule. Dual-mode silicon, single-mode lab.

**Blueprint corrections:** draft P19 drove the SIM7070 from an ESP32 as the application processor. Valid with flying UART leads, but then it stops being an embedded-Linux lab. We keep the Pi 3B+ as the host and use the module's built-in MQTT + GNSS AT set. Power GNSS down during transmission.

**Intent:** lock the SIM7070G to Cat-M (AT+CMNB=1). Every publish carries a GNSS time, not `date` from the Pi.

**Kit:** Pi 3B+, SIM7070G HAT + LTE and GNSS antennas + Cat-M SIM, yellow LED BCM22.

**Wiring:** same seating rules as P01. USB preferred for AT + NMEA. VCCIO=3V3, auto PWR. GNSS antenna needs sky view. No second HAT.

**Steps:**
1. AT bring-up on the USB AT port:
```
sudo minicom -D /dev/ttyUSB2 -b 115200
AT+CPIN?
AT+CNMP=38          # LTE only
AT+CMNB=1           # Cat-M, NOT NB-IoT
AT+CPSI?            # must mention CAT-M / eMTC
AT+CSQ
```
2. GNSS with radio courtesy:
```
AT+CGNSPWR=1
# wait, outdoors
AT+CGNSINF
# before MQTT TX:
AT+CGNSPWR=0
```
3. Module MQTT (keeps TLS/session on the modem, not in Python):
```
AT+SMCONF="URL","test.mosquitto.org",1883
AT+SMCONF="CLIENTID","p03-catm"
AT+SMCONN
AT+SMPUB="lab/p03/fix",0,1
# type payload:  2026-09-14T10:00:00Z,lat,lon
```
4. Automate: a 30-line Python script talks AT over serial.Serial, parses +CGNSINF, powers GNSS off, publishes, sleeps 60 s.

**Acceptance test:** AT+CPSI? contains CAT-M. The broker payload starts with a GNSS stamp, not the Pi clock. A module that silently falls to GPRS fails the lab, fix AT+CMNB before continuing.

**Practices:** cache the last good fix. Do not block MQTT on a cold GNSS start. The same HAT cannot serve P02.

---

# P04 - STWIN.box vibration and ultrasound USB gateway

**Unique:** owns the only 6 kHz vibration sensor (IIS3DWB) and the only ultrasound / industrial MEMS mics. USB-CDC into Linux. DSI glass is the waterfall. No HAT.

**Blueprint corrections:** draft P1 called USB-C a "VCP" and invoked Intel MKL. USB-C is the connector; CDC-ACM exists only after HSDatalog / USB-CDC firmware is present. Use a NumPy FFT. Draft P20 reused STWIN as a BLE factory tier, which would double-own this silicon. Anomaly-only uplink is a policy in this lab, not a second project.

**Intent:** the STWIN.box is a USB industrial probe. The Pi 4 isolates one CPU core for the FFT thread (isolcpus + taskset) and classifies NORMAL / WARNING / FAULT from spectral peaks. Touch the case to raise vibration energy; speak to raise mic RMS without moving the vibration band.

**Kit:** Pi 4, official DSI touchscreen, STWIN.box, USB-C cable. Optional STWIN LiPo only if it already sits in the kit box.

**Architecture:**
```
IIS3DWB --SPI--> STM32U585 --USB-C--> Pi 4 /dev/ttyACM*
IMP34DT05 / IMP23ABSU --I2S/analog--> STM32U585 --same CDC-->
Pi: serial reader -> ring buffer -> window+FFT -> peak bins
     -> NORMAL | WARNING | FAULT  (DSI waterfall)
Boundary: STM32 does DMA + timestamp + sequence + packetize.
          Linux does DSP + policy. Do not FFT on the M33.
```

**Wiring:**
```
STWIN USB-C  --->  Pi 4 USB-A     (data + 5V bus power)
DSI ribbon   --->  Pi 4 DISPLAY   (not the 40-pin)
40-pin left empty. Strain-relieve the USB cable.
Mount STWIN on the machine under test, not on a ringing desk if you
can avoid it --- the desk is a valid first stimulus.
```

**Steps:**
1. On a PC or the Pi, flash STWIN with ST HSDatalog or the Zephyr steval_stwinbx1 sensors sample (USB CDC). STM32CubeProgrammer in USB DFU mode on the same USB-C:
```
lsusb | grep -i st
# hold BOOT / DFU as in UM of STWIN.box, then
STM32_Programmer_CLI -c port=USB1 -w firmware.bin 0x08000000
```
2. Isolate a core so CFS does not migrate the FFT thread:
```
# /boot/firmware/cmdline.txt append:
#   isolcpus=3 nohz_full=3
sudo reboot
ls /dev/ttyACM*
sudo apt-get install -y python3-serial python3-numpy python3-matplotlib
```
3. Ingest. If the firmware is the Zephyr text dashboard, parse lines. If it is HSDatalog binary, use ST's host decoder first, do not invent a frame format.
```
stty -F /dev/ttyACM0 921600 raw -echo
taskset -c 3 python3 p04_fft.py
```
4. p04_fft.py in outline: read 2048 int16 samples at the configured ODR, Hann window, numpy.fft.rfft, take magnitude, compare a high-frequency band against a baseline captured on an idle machine. Print NORMAL/WARNING/FAULT.
5. Optional policy: publish only FAULT (anomaly-only uplink). That is the draft-P20 idea, implemented here as a flag.

**Acceptance test:** touch the STWIN case, vibration-band energy rises. Speak near the digital mic, RMS rises and the vibration band does not. That orthogonality is the lab. A 2-second capture file has a monotonic sequence number and no dropped frames.

**Practices:** short USB cable. No Wi-Fi scan during a capture. Timestamp with CLOCK_MONOTONIC. USB-powered is the Linux-gateway mode; the LiPo is for standalone logging you are not doing today.

---

# P05 - MCC 118 100 kS/s voltage recorder

**Unique:** owns the only calibrated 10 V analog path. Not an IMU lab.

**Blueprint corrections:** draft P2 and P17 claimed Linux IIO DMABUF + io_uring on this HAT. MCC 118 talks SPI to the official daqhats library. There is no mainline IIO driver and no IIO_BUFFER_DMABUF_ATTACH_IOCTL. Draft P17 also closed a PID loop onto a green LED, which is a demo, not a process plant. We record voltages correctly first.

**Intent:** eight single-ended 12-bit inputs, 100 kS/s board maximum, factory calibration in EEPROM. Pi 4 plus DSI is the scope. POW-BB 3.3 V on CH0 is the known reference.

**Kit:** Pi 4, MCC 118, official DSI touchscreen, POW-BB, jumpers.

**Wiring:**
```
MCC 118 seated ALONE on the 40-pin
  uses SPI0 CE0, ID EEPROM, address GPIO12/13/26
  address jumpers all OFF = board 0

Screw terminals:
  CH0  ---- POW-BB 3V3     expect ~3.30 V
  CH1  ---- POW-BB 5V      only because 5 < 10.1
  AGND ---- POW-BB GND     common analog ground
  DGND ---- Pi GND
Never exceed +/-10.1 V. Pi GPIO is not an analog source.
Warm up 1 minute before quoting a number.
```

**Steps:**
1.
```
git clone https://github.com/mccdaq/daqhats.git
cd daqhats && sudo ./install.sh
sudo daqhats_read_eeproms
daqhats_list_boards
```
2. Single-shot sanity:
```
from daqhats import mcc118, hat_list, HatIDs
print(hat_list(filter_by_id=HatIDs.MCC_118))
h = mcc118(0)
print([round(h.a_in_read(ch), 4) for ch in range(8)])
```
3. Hardware-paced scan (this is the product, not the single read):
```
from daqhats import mcc118, OptionFlags
h = mcc118(0)
h.a_in_scan_start(0x03, 10000, 10000, OptionFlags.DEFAULT)
block = h.a_in_scan_read(20000, 5000)
print(len(block.data), block.data[:8])
h.a_in_scan_stop(); h.a_in_scan_cleanup()
```
4. Write a 1-second 10 kS/s CH0+CH1 file as CSV with a monotonic sample index. Plot on the DSI with matplotlib.

**Acceptance test:** CH0 reads 3.30 plus or minus 0.05 V against POW-BB. A 10 kS/s by 1 s file has 10 000 samples per enabled channel and no holes.

**Practices:** common AGND. Do not stack the LCD or Explorer700, both want SPI. 100 kS/s is the board cap, budget CPU if you enable all eight. A closed-loop PWM-to-LED demo is a stretch only after the file is honest.

---

# P06 - 8x8 ToF occupancy kiosk

**Unique:** owns VL53L8CX multi-zone ranging and the Waveshare SPI LCD as a local heat map. Not an IMU and not V4L2.

**Blueprint corrections:** draft P8 loaded stsw-img042 as a V4L2 camera node and drove the LCD with flexfb/fbtft. On Bookworm the Waveshare 3.5 inch (A) uses the vendor overlay / DRM path, and ST's ToF Linux stack is not a Pi camera. Stream 64 millimetre values over the Nucleo USB-CDC.

**Intent:** Nucleo + 53L8A1 produce an 8x8 millimetre grid. Pi 3 + 3.5 inch LCD paint occupancy / desk presence.

**Kit:** Pi 3, Waveshare 3.5 inch RPi LCD (A), Nucleo-H7A3ZI-Q, X-NUCLEO-53L8A1, USB cable.

**Wiring:**
```
53L8A1 stacked on Nucleo Arduino UNO headers.
J9 SPI_I2C_N = I2C (pins 2-3) unless you deliberately choose SPI.
Nucleo USER USB or ST-LINK USB --> Pi USB-A   (CDC ACM)
Waveshare 3.5" LCD seated on Pi 3 40-pin (this lab's HAT).
Two hosts, one USB cable between them.
Point the aperture at 200-1500 mm of free space; no sunlight in the FOV.
Cover glass + 0.5 mm spacer as shipped.
```

**Steps:**
1. Nucleo firmware: STM32duino X_NUCLEO_53L8A1_HelloWorld_I2C or the Cube VL53L8CX example. Print one line per frame: `Z,d00,d01,...,d63` in millimetres.
2. LCD on Bookworm: follow the current Waveshare 3.5 inch (A) note for Bookworm (vendor dtoverlay, not the 2016 flexfb). Confirm a framebuffer exists (/dev/fb0 or a DRM card).
3. Consumer on the Pi:
```
import serial
ser = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
while True:
    line = ser.readline().decode(errors="ignore").strip()
    if line.startswith("Z,"):
        z = [int(x) for x in line.split(",")[1:]]
        print(min(z), max(z), "mm", len(z))
```
4. Paint an 8x8 rectangle grid on the framebuffer. Occupied = range below a threshold you measure against an empty FOV.

**Acceptance test:** a hand at 300 mm occupies 4 to 10 zones. An empty FOV reports the wall at a stable range plus or minus 20 mm. The LCD shows a coarse heat map, 64 values every frame.

**Practices:** one Arduino shield on the Nucleo. Do not also stack IKS4A1 or IKS5A1.

---

# P07 - Consumer 9-DoF plus climate (IKS4A1)

**Unique:** owns consumer MEMS + SHT40 humidity + the Qvar electrode. Wearable physics, not industrial high-g.

**Intent:** IKS4A1 on the Nucleo streams JSON of LSM6DSV16X / LSM6DSO16IS, LIS2MDL, LIS2DUXS12, LPS22DF, SHT40, STTS22H. Linux records.

**Kit:** Pi 4 or 3B+, Nucleo-H7A3ZI-Q, X-NUCLEO-IKS4A1, USB cable.

**Wiring:**
```
IKS4A1 on Nucleo Arduino headers.
UM3239 Mode 1 (all sensors on uC I2C):
  J4: 1-2 and 11-12
  J5: 1-2 and 11-12
Nucleo USB --> Pi.
sudo i2cdetect on the *Pi* will NOT see these chips.
They live behind the STM32. That is the point.
```

**Steps:**
1. Flash an STM32duino X-NUCLEO-IKS4A1 sample, or a sketch that prints one JSON object per 20 ms:
```
{"t":24.1,"rh":41.2,"p":1013.2,"ax":0.02,"ay":0.01,"az":1.00}
```
2. On the Pi, `cat /dev/ttyACM0 | tee p07.jsonl`.
3. Compute a running tilt from acc+gyro only. Magnetometer heading waits on a hard-iron calibration you actually perform. Do not publish a raw heading and call it fusion.
4. Qvar swipe stays off until the IMU stream is clean. Then enable the electrode jumpers (UM3239 Qvar mode) as a second session, not a kitchen sink.

**Acceptance test:** the firmware scanner sees 0x6A, 0x1E, 0x44, 0x5C, 0x38-class addresses. Breathing on SHT40 moves RH. A palm over the board moves STTS22H by about 0.5 degrees C.

**Practices:** Mode 1 is the Linux-friendly setup. Sensor-hub / ISPU modes hide devices behind the LSM6, useful after the JSON is honest.

---

# P08 - Industrial high-g and dual-scale baro (IKS5A1)

**Unique:** owns ISM6HG256X simultaneous low-g/high-g and ILPS22QS 1260/4060 hPa. No humidity. A different instrument from P07.

**Blueprint corrections:** draft P13 put an ISM330DHCX Machine Learning Core on the IKS5A1. That IMU is on the STWIN.box. IKS5A1 has ISM330IS (ISPU) and ISM6HG256X. Use the chips on the board.

**Intent:** two concurrent streams, a low-g IMU at 104 Hz and a high-g peak-hold. Barometer in the 4060 hPa scale only when you actually pressurise.

**Kit:** Pi 3B+, Nucleo-H7A3ZI-Q, X-NUCLEO-IKS5A1 (IKS4A1 removed), USB.

**Wiring:** the same physical stack as P07 with the other shield. Default I2C: ISM6HG256X 0x6A, ISM330IS 0x6B, ILPS22QS 0x5C, IIS2MDC 0x1E, IIS2DULPX 0x19.

**Steps:**
1. STM32duino X-NUCLEO-IKS5A1 samples for ISM6HG256X 6D / wake-up and ILPS22QS pressure.
2. Publish two topics or two JSON keys: `lowg` and `highg_peak`. Never collapse them into one "acc" field.
3. Drop-test protocol: 20 mm onto a book, not onto concrete.

**Acceptance test:** on the drop the high-g peak fires and the low-g saturates. Quiet-room ILPS22QS noise is a few Pa. The numbers live in a different log file from P07.

**Practices:** mixing the P07 and P08 JSON schemas is how fake "9-DoF industrial wearables" get invented. Do not.

---

# P09 - NanoPi NEO Air headless hub and honest eMMC logging

**Unique:** owns the only non-Raspberry Linux host and the only onboard eMMC wear discussion. 24-pin I2C0 + AP6212 Wi-Fi.

**Blueprint corrections:** draft P4 and P12 seated 40-pin cellular HATs on this board. They do not fit. Draft P6 framed ADXL at 100 Hz as "vibration logging"; that physics belongs to P04. Here the product is the logger and the filesystem, not the spectrum.

**Intent:** FriendlyElec Ubuntu on eMMC. ADXL345 on I2C0. Headless MQTT over onboard Wi-Fi. An optional F2FS data partition for append-heavy logs (the only honest use of the draft-P6 idea).

**Kit:** NanoPi NEO Air, SEN0032 ADXL345, Renkforce USB-TTL on UART0, 5 V via micro-USB or pin 2.

**Wiring:**
```
NEO Air 24-pin            ADXL345
1  SYS_3V3 -------------- VCC
3  I2C0_SDA ------------- SDA
5  I2C0_SCL ------------- SCL
6  GND ------------------ GND

UART0 4-pin: GND, 5V, TX, RX --> USB-TTL at 3.3 V logic, 115200 8N1
Do not put 5 V onto SYS_3V3.
```

**Steps:**
1. Flash FriendlyElec Ubuntu to eMMC (vendor wiki). First login on UART0.
2.
```
sudo apt-get install -y i2c-tools python3-smbus mosquitto-clients
ls /dev/i2c-*
sudo i2cdetect -y 0         # expect 53
nmcli device wifi connect "YOURAP" password "..."
```
3. Reuse the P01 publisher with bus number 0.
4. Optional F2FS data partition, only if you are willing to repartition unused eMMC space, not the rootfs you just booted:
```
lsblk
# mkfs.f2fs /dev/mmcblk2pX     # X = unused partition YOU created
# mount -o discard /data
# append-only log files, fsync on a timer, not per sample
```

**Acceptance test:** unplug the USB-TTL; the board stays up on Wi-Fi and publishes. `free -h` shows about 512 MB. No desktop.

**Practices:** 5 V at 2 A. The AP6212 shares Wi-Fi and BT. Leave the ESP8266 for P17.

---

# P10 - PPK2 energy characterisation bench

**Unique:** owns microamp figures. No telemetry product.

**Blueprint corrections:** draft P9 put the PPK2 in series with a SIM7070 HAT that was still powered from the Pi 5 V pins. That measures nothing useful. Either lift the HAT 5 V pins and feed VOUT into the modem power input, or measure a flying-lead DUT (ESP32) that is not also USB-powered. PPK2 source mode is 5 V-class and about 1 A; an LTE TX peak can exceed that, so do not source a Cat-4 PA from the PPK2.

**Intent:** three firmware states on the ESP32: radio-off idle, BLE advertise, deep-sleep. The Pi 4 logs the PPK2 stream.

**Kit:** Pi 4, PPK2, ESP32 NodeMCU as DUT, POW-BB, jumpers.

**Wiring:**
```
PPK2 VIN   <--  Pi USB or bench 5V
PPK2 GND   ---  DUT GND --- Pi GND (common)
PPK2 VOUT  -->  ESP32 3V3 pin     source mode, set 3300 mV
PPK2 USB   -->  Pi USB-A
ESP32 USB  DISCONNECTED           this is the whole experiment
```

**Steps:**
1. Install Nordic's nRF Util / Power Profiler CLI on the Pi, or capture CDC samples with the published ppk2-python helpers.
2. Write three tiny ESP32 sketches. Do not measure the Arduino blink sketch and call it deep-sleep.
3. Capture 100 kS/s for 2 s per state. Record median and p95.

**Acceptance test:** deep-sleep is tens of microamps to low milliamps on a dev-board (USB-UART silicon leaks, write that sentence in the lab book). Active radio is tens of milliamps. If the two traces overlay, the DUT is still USB-powered.

**Practices:** quote median and p95, not one screenshot. Never source SIM7600 TX from the PPK2.

---

# P11 - Explorer700 field console (DTO, RTC, OLED, IR)

**Unique:** owns DS3231 as hwclock, the SSD1306 OLED, IR, joystick, BMP280 and PCF8591. The Device Tree overlay lab lives here because the chips do.

**Blueprint corrections:** draft P7 is this lab. Draft P18 then stacked the same HAT with a SIM7600. Forbidden. Time-series store-and-forward without the second HAT is: log locally, copy off on Ethernet later.

**Intent:** the Pi 3B+ boots, loads an overlay that binds DS3231 and BMP280, sets system time from the RTC, and puts status on the OLED. Joystick and IR are local operator inputs.

**Kit:** Pi 3B+, JOY-iT RB-Explorer700. No other 40-pin board.

**Wiring:** seat Explorer700 on the Pi 3B+. No flying leads required for the onboard chips. I2C1: DS3231 0x68, BMP280 0x76 or 0x77, PCF8591 0x48, PCF8574 0x20.

**Steps:**
1.
```
sudo apt-get install -y i2c-tools python3-smbus python3-luma.oled
sudo i2cdetect -y 1
# expect 68, 76 or 77, 48, 20
```
2. An overlay rather than userspace-only bitbang. Example fragment for /boot/firmware/config.txt:
```
dtparam=i2c_arm=on
dtoverlay=i2c-rtc,ds3231
# BMP280 can be bound with a custom overlay or iio userspace
```
3. Set and persist time:
```
timedatectl
sudo hwclock -w
sudo hwclock -r
# disconnect Ethernet/Wi-Fi, reboot, confirm the clock survived
```
4. OLED: the luma.oled SSD1306 SPI example from the JOY-iT manual.
5. Joystick + IR: vendor examples under RB-Explorer700. Map the joystick to a menu, IR to a keycode, the buzzer to an ack.

**Acceptance test:** after a power pull with no network, `date` is still correct within 2 s. The OLED shows the host name + RTC. i2cdetect still shows 0x68.

**Practices:** this HAT is the RTC source for any later store-and-forward idea. Do not steal it onto a cellular Pi.

---

# P12 - Official DSI operator glass and MQTT broker

**Unique:** owns the 7 inch DSI panel, the keyboard, and the broker. No 40-pin HAT, so any USB gadget from P04/P16/P18 can sit beside it.

**Blueprint corrections:** draft P14 is this lab plus ESP32 actuation. Keep the actuation in P16 so this chapter stays a console/broker lab.

**Intent:** Pi 4 + official touchscreen runs labwc/Wayland (the Bookworm default) and Mosquitto. A full-screen page shows fleet topics. The keyboard is the operator input.

**Kit:** Pi 4, official RPi touchscreen on DISPLAY, official keyboard.

**Steps:**
1. Seat the DSI ribbon with the Pi powered off. Power on, confirm the desktop on the glass (Bookworm detects the official panel).
2.
```
sudo apt-get install -y mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto
mosquitto_sub -h localhost -t 'lab/#' -v
```
3. Point a full-screen Chromium at a local file:///home/pi/p12/dash.html that subscribes via websockets (mosquitto listener 9001), or keep it simpler: three `watch` terminals tiled on the glass.
4. Lock the broker to the LAN:
```
# /etc/mosquitto/conf.d/lab.conf
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
```

**Acceptance test:** a publish from another host on the LAN appears on the glass in under 1 s. A reboot survives the broker unit.

**Practices:** this host is allowed to wear a HAT in a later sitting. Today it does not.

---

# P13 - Dual-radio signalling failover

**Unique:** owns multi-homing. Two Pis, two modems, one MQTT topic with a source tag. Not link bonding, and not a stacked-HAT fantasy.

**Intent:** Pi 4 + SIM7600 is the primary path (reuse the P01 WAN). Pi 3 + SIM7020 is the backup signalling path (reuse P02). A supervisor on the P12 broker marks PRIMARY or FAILOVER.

**Kit:** Pi 4 + SIM7600 (from P01), Pi 3 + SIM7020 (from P02), the P12 broker if built, else test.mosquitto.org.

**Steps:**
1. Do not undress P01 and P02 on the same afternoon if you still need their individual acceptance tests. This lab assumes both already attach.
2. On the Pi 4, a 20-line script publishes `lab/p13/health {"src":"cat4","ok":1}` every 15 s via usb0.
3. On the Pi 3, a script publishes the same topic `{"src":"nb","ok":1}` every 120 s via CoAP/UDP or whatever P02 actually reached.
4. Supervisor (on P12 or a laptop): if no `cat4` sample for 45 s, state = FAILOVER and the NB sample is the live one.
5. Pull the LTE antenna. The state must flip. Refit the antenna. The state returns to PRIMARY.

**Acceptance test:** the antenna-pull test above. Logs show both sources, never a bonded `metric 0` default route on one kernel.

**Practices:** this is signalling failover. Do not advertise a 50 Mbit NB-IoT backup, it does not exist.

---

# P14 - USB-TTL provisioning jig

**Unique:** owns the Renkforce cable as a factory tool, plus both ESP boards as targets. Not a sensor lab.

**Intent:** the Pi 3 flashes and AT-provisions the ESP32 and ESP8266 without either board becoming the product.

**Kit:** Pi 3, Renkforce USB-TTL, ESP32 NodeMCU, ESP8266-PROG, R/Y/G LEDs, breadboard.

**Wiring:**
```
USB-TTL 3V3  --> ESP VIN/3V3   (jumper the adapter to 3.3 V)
USB-TTL GND  --> ESP GND
USB-TTL TXD  --> ESP RX
USB-TTL RXD  --> ESP TX
ESP32: hold GPIO0 low for download, EN reset
ESP8266-PROG: onboard USB-UART may already exist --- use ONE
              UART path, not both at once
```

**Steps:**
1.
```
sudo apt-get install -y python3-serial esptool minicom
esptool.py --chip esp32 --port /dev/ttyUSB0 chip_id
esptool.py --chip esp8266 --port /dev/ttyUSB0 chip_id
```
2. Flash a known AT or blink image. Confirm the yellow LED script you wrote, not a random vendor blob you cannot reset.
3. Record a provisioning checklist: MAC, flash size, a unique client_id, Wi-Fi credentials written to NVS.

**Acceptance test:** both chips report chip_id. A second run after a power pull still boots the image you flashed.

**Practices:** 3.3 V logic. Never 5 V TTL into ESP pins.

---

# P15 - POW-BB mixed-voltage discipline

**Unique:** owns the electrical safety lab. No radio, no IMU.

**Intent:** label the rails. Prove that a 5 V LED circuit does not share a data pin with a 3.3 V SoC input. Red/yellow/green as a semaphore driven from 3.3 V GPIO through resistors already on the LED module or a series resistor on the breadboard.

**Kit:** any Pi or NanoPi, POW-BB, breadboard, jumpers, R/Y/G LEDs.

**Wiring:**
```
PSU 5V ---- POW-BB 5V rail   (label it)
POW-BB 3V3 rail labelled     (from the board regulator, not a guess)
GND common
LED anodes on 3V3 via series R, cathodes to GPIO as sinks
  OR LED modules already 3.3 V tolerant on the data pin
Never tie a 5V pull-up to a Pi GPIO.
```

**Steps:**
1. Multimeter: the 5 V rail is 5 V, the 3.3 V rail is 3.3 V, GND is common. Write the three numbers in the lab book.
2. Blink GRN/YEL/RED from BCM 27/22/23 (Pi) without ever connecting those GPIOs to the 5 V rail.
3. Negative test (on the breadboard only, not on a GPIO): show that a 5 V pull-up sits above 3.3 V. Then take it apart.

**Acceptance test:** three measured rail voltages. A blink sketch. A sentence in the lab book that says "no 5 V on header pins".

---

# P16 - ESP32 Wi-Fi/BLE sidecar

**Unique:** owns the sidecar pattern: Linux keeps Ethernet/LTE, the ESP32 keeps the 2.4 GHz radio personality. The actuator LEDs from the P12 broker live here.

**Blueprint corrections:** draft P14 mixed the glass and the actuator. Split them.

**Intent:** the ESP32 joins the Pi 4 AP or LAN, subscribes to `lab/p16/led/#`, and drives R/Y/G. The Pi never bit-bangs those LEDs.

**Kit:** Pi 4 (the P12 broker is welcome), ESP32, POW-BB, LEDs, USB for power and serial.

**Steps:**
1. An ESP32 Arduino or ESP-IDF sketch: Wi-Fi STA, MQTT client, three GPIOs.
2. Publish from the Pi:
```
mosquitto_pub -h localhost -t lab/p16/led/red -m 1
```
3. Optional BLE: the ESP32 advertises a lab UUID; the Pi uses bluetoothctl only as a presence check. Do not build a phone app.

**Acceptance test:** a topic flip toggles the matching LED in under 200 ms on a quiet LAN.

---

# P17 - ESP8266 AT modem on the NanoPi, onboard Wi-Fi off

**Unique:** owns the air-gap radio. The NanoPi's AP6212 is down and the ESP8266 speaks AT over UART1. Different from P16, where the ESP32 is a peer, not an AT slave.

**Wiring:**
```
NEO Air UART1 TX (pin 8 / GPIOG6)  --> ESP8266 RX
NEO Air UART1 RX (pin 10 / GPIOG7) --> ESP8266 TX
GND common, 3V3 from SYS_3V3
nmcli radio wifi off            # AP6212 down
```

**Steps:**
```
sudo minicom -D /dev/ttyS1 -b 115200
AT
AT+CWMODE=1
AT+CWJAP="YOURAP","..."
AT+CIFSR
```

**Acceptance test:** iwconfig / nmcli on the NanoPi shows Wi-Fi off, yet AT+CIFSR returns an IP on the ESP. That split is the lab.

---

# P18 - Nucleo USB-CDC recorder (optional EKF on the M7)

**Unique:** owns high-rate USB gadget traffic from the bare Nucleo. Linux is the recorder. An EKF may run on the H7 FPU; it must not replace the Linux host.

**Blueprint corrections:** draft P15 "discards Linux entirely". Out of scope for an embedded-Linux book unless a host still captures the UART/CDC stream. We keep the host.

**Intent:** the Nucleo-H7A3ZI-Q streams a packed binary or text sample at 1 kHz or more over USB-CDC. The Pi 4 writes a file with sequence numbers. Optional: stack the IKS4A1 and run a quaternion EKF on the M7, still logged by Linux.

**Kit:** Pi 4, Nucleo-H7A3ZI-Q, USB cable, optional IKS4A1, optional Renkforce USB-TTL as a second console.

**Steps:**
1. A Cube / Arduino USB-CDC sketch: increment a uint32 sequence, print it. Aim for 1 kHz.
2. Pi:
```
stty -F /dev/ttyACM0 921600 raw
python3 p18_sink.py          # counts gaps in the sequence
```
3. Optional EKF session: reuse the P07 shield, compute a quaternion on the M7 FPU, print `seq,q0,q1,q2,q3`. Linux does not run the filter.

**Acceptance test:** a 10-second capture at 1 kHz has 0.1 percent or fewer sequence gaps. lsusb shows the CDC interface.

---

# P19 - SPI LCD tilt meter (offline instrument)

**Unique:** owns the 3.5 inch LCD as a field instrument with ADXL345 tilt. No broker, no radio. The LCD owns the 40-pin, so this cannot run with P06 on the same Pi at the same time, a different sitting.

**Intent:** Pi 3 + Waveshare 3.5 inch (A) + ADXL345. Pitch and roll on the glass. Battery-unaware: this is an offline meter, not a gateway.

**Kit:** Pi 3, 3.5 inch LCD (A), SEN0032 ADXL345, jumpers to the HAT pass-through 3V3/GND/SDA/SCL.

**Steps:**
1. Seat the LCD. Confirm the vendor overlay as in P06.
2. Wire the ADXL345 to pins 1/3/5/6 if the LCD HAT pass-through exposes them (it usually does). `i2cdetect -y 1` shows 0x53.
3. Python: read ax, ay, az, compute pitch = atan2(ax, sqrt(ay^2 + az^2)), roll = atan2(ay, sqrt(ax^2 + az^2)), draw two bars on the framebuffer.

**Acceptance test:** rotate 90 degrees on one axis: that bar travels full scale, the other stays near zero. No MQTT client is running.

---

# P20 - Four-host fleet health

**Unique:** owns orchestration across the Pi 4, Pi 3B+, Pi 3 and NanoPi. No new physics.

**Intent:** from P12 (or any Pi with SSH keys), a 40-line script pings all four hosts and records uname, free RAM, which 40-pin HAT EEPROM answers, and which USB gadgets are present.

**Steps:**
1. SSH keys from the operator Pi to the other three.
2. Script outline:
```
for h in pi4 pi3bp pi3 air; do
  ssh $h 'hostname; uptime; free -h; vcgencmd measure_temp 2>/dev/null
          i2cdetect -y 1 2>/dev/null | head
          lsusb | grep -iE "st|sim|nordic|1a86|10c4"'
done
```
3. Extra: detect the HAT EEPROM at the ID bus (/proc/device-tree/hat on a Pi). Report "none" honestly when the header is empty.
4. Write the output to /var/log/lab-fleet.txt on a timer. That file is the acceptance artifact.

**Acceptance test:** four hostnames. A HAT that is physically seated is named. A HAT that is on the shelf is reported absent. No step in this lab reconfigures a modem.

---

# Lab sequence and teardown

Work in this order if you are starting from bare boards:

1. P15 rails (do not skip).
2. P14 jig, then P16 and P17 sidecars.
3. P09 NanoPi bring-up.
4. P11 Explorer700 RTC (then take the HAT off).
5. P12 operator glass.
6. P05 MCC 118, P19 LCD tilt, P06 ToF kiosk, each tears the 40-pin down after acceptance.
7. P07 then P08 (swap the Nucleo shield), P18 recorder, P04 STWIN.
8. P01, P03, P02 cellular, then P13 failover.
9. P10 energy numbers on whatever DUT you still trust.
10. P20 last, so the fleet file reflects a known sitting.

**Teardown:** power off before unseating a HAT. Discharge curiosity, not the modem PA: refit antennas before the next transmission. Put IKS4A1 and IKS5A1 back in separate bags, they look similar from across the bench.

# Reference AT and addresses

- **SIM7600E-H (P01, P13):** AT, AT+CPIN?, AT+CSQ, AT+COPS?, AT+CEREG?, AT+CGPS=1, AT+CGPSINFO
- **SIM7020E (P02, P13):** AT+CBAND, AT+CEREG?, AT+CPSMS, AT+CEDRXS, AT+CSOC, AT+CSOCON, AT+CSOSEND
- **SIM7070G (P03):** AT+CNMP=38, AT+CMNB=1, AT+CPSI?, AT+CGNSPWR, AT+CGNSINF, AT+SMCONF, AT+SMCONN, AT+SMPUB
