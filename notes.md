[Get platformio](https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py)

```
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py; python3 get-platformio.py
```

https://learn.adafruit.com/adafruit-esp32-feather-v2/arduino-ide-setup
https://github.com/arduino/Arduino/wiki/Unofficial-list-of-3rd-party-boards-support-urls#list-of-3rd-party-boards-support-urls


> https://espressif.github.io/arduino-esp32/package_esp32_index.json
> https://adafruit.github.io/arduino-board-index/package_adafruit_index.json

To install the CP2104 / CP2102N USB driver, you'll need to download the appropriate driver package from Silicon Labs https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers? tab=downloads and then manually install it using Windows Device Manager. 
Here's a step-by-step guide:
1. Download the Driver:
Go to Silicon Labs' website https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers? tab=downloads
Download the appropriate driver package (VCP Driver Kit) for your operating system (Windows). 
2. Extract the Driver:
Unzip the downloaded driver package to a directory on your computer. 
3. Install the Driver:
Option 1: Using Device Manager:
Plug in the device with the CP2104/CP2102N chip. 
Open Device Manager. 
Look for the device listed under "Other devices" or "Unknown devices" (since the driver isn't installed yet). 
Right-click on the device and select "Update driver software". 
Choose "Browse my computer for driver software". 
Browse to the directory where you extracted the driver files and select the appropriate driver (usually a .inf file). 
Follow the on-screen instructions to complete the installation. 
Option 2: Using the .inf file:
Navigate to the folder where you extracted the driver files. 
Locate the .inf file (usually named something like "silabser"). 
Right-click on the .inf file and select "Install". 
Follow the on-screen instructions to complete the installation. 
4. Verify Installation:
Once the installation is complete, the device should appear under "Ports (COM & LPT)" in Device Manager.
You should see a COM port listed, indicating that the driver is installed and the device is recognized.



To estimate position using **time-of-flight (ToF)**, the distance $d$ a signal travels is determined by:

$$
d = c \cdot t
$$

Where:
- $d$ = distance
- $c$ = speed of light $\approx 3 \times 10^8 \, \text{m/s}$
- $t$ = time (seconds)

Any **error in time measurement** $\Delta t$ results in a **distance error** $\Delta d$:

$$
\Delta d = c \cdot \Delta t
$$

Solving for the **required time resolution** $\Delta t$ to achieve a given **positioning accuracy** $\Delta d$:

$$
\Delta t = \frac{\Delta d}{c}
$$

---

## 🎯 Example Calculations

### 1. Accuracy Within 10 cm (0.1 m)

$$
\Delta t = \frac{0.1}{3 \times 10^8} = 3.33 \times 10^{-10} \, \text{seconds}
$$

- 333 picoseconds
- Clock frequency needed: ~3 GHz

---

### 2. Accuracy Within 100 ft (30.48 m)

$$
\Delta t = \frac{30.48}{3 \times 10^8} \approx 1.02 \times 10^{-7} \, \text{seconds}
$$

- 101.6 nanoseconds
- Clock frequency needed: ~10 MHz

---

## 📊 Summary Table

| **Target Accuracy** | **Distance (m)** | **Required Clock Precision** ($\Delta t$) | **Time Unit**        |
|---------------------|------------------|--------------------------------------------|-----------------------|
| 0.1 m (10 cm)       | 0.1              | $3.33 \times 10^{-10} \, \text{s}$         | 333 ps               |
| 1.0 m               | 1.0              | $3.33 \times 10^{-9} \, \text{s}$          | 3.3 ns               |
| 10 m                | 10               | $3.33 \times 10^{-8} \, \text{s}$          | 33 ns                |
| 30.5 m (100 ft)     | 30.5             | $1.02 \times 10^{-7} \, \text{s}$          | 101.6 ns             |

---

## 🧠 Rule of Thumb

To achieve accuracy of $X$ meters, time resolution should be:

$$
\Delta t \approx \frac{X}{3 \times 10^8}
$$

To convert this into **required clock frequency**:

$$
f = \frac{1}{\Delta t}
$$

Example: For $X = 0.1 \, \text{m}$ (10 cm):

$$
\Delta t = 3.33 \times 10^{-10} \Rightarrow f \approx 3 \, \text{GHz}
$$

---

## 🛰️ Notes on Clock Synchronization

- If using **one-way ToF**, clocks must be **synchronized** to this level of precision.
- If using **two-way ranging (TWR)**, only relative time measurement is needed, reducing dependency on absolute clock sync.
- **Ultra-Wideband (UWB)** modules like **DWM1000** or **DWM3000** perform this internally with timestamping accuracy in the **10–15 cm range**.

