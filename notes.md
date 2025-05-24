https://learn.adafruit.com/adafruit-esp32-feather-v2/arduino-ide-setup
https://github.com/arduino/Arduino/wiki/Unofficial-list-of-3rd-party-boards-support-urls#list-of-3rd-party-boards-support-urls

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