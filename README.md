# Wildfire Sensor Detection Network
This project contains source code used for our EE/CE Capstone project.

## Project Structure

### backend/
This contains the python backend for our base station PC. 

### firmware/
This contains source code for several isolated projects used during sensor bringup and validation. This also contains the source code for our final integrated firmware project, which can be found in `firmware/sensor_integration`.

### frontend/
This contains the source code for our front-end UI. In order to run this, navigate to `frontend/` and run `npm run dev`.

### LoRa/
This contains the source code for the p2p LoRa setup used during demos. It also contains .zip files with the source code for the mesh network setup.

### models/
This directory contains scripts for the custom CNN implementation, as well as the fine-tuned ResNet50 implementation, used for the drone's image classification.

## Usage Instructions
### Sensor Module -> Base Station PC
In order to run this project, you will need the following:
- An STM32L476rg development board (We used a Nucleo L476rg) flashed with the project in `firmware/sensor_integration`. 
- Two RAK4631 LoRa modules

#### Steps
1. Plug in the LoRa module flashed with the `LoRa/p2p_projects/capstone` project to the STM32 development board.
2. Plug in the LoRa module flashed with the `LoRa/p2p_projects/capstone 2 rx` project to the base station PC.
3. Navigate to `backend/` and run `python3 data_intake_updated /path/to/lora/usbport`. This will start the reception of LoRa messages, and they will be posted to the MongoDB database.
4. While in `backend/`, also run `python3 server.py`. This starts the back-end server, which posts recent readings to the front-end UI.
5. Navigate to `frontend/` and run `npm run dev` to start the web-based UI.

### Drone -> Base Station PC
In order to run this project with the drone stream as well, you will need:
- A Raspberry Pi with an Arducam IMX 780 camera attached, flashed with the latest OpenHD software.
- OpenHD Ground installed on your base station PC.
- Two OpenHD-compliant WiFi adapters and the correct driver for said adapter installed on your base station PC.

Note that running the drone code via OpenHD will mean that your base station PC will not be able to connect to internet. In order to also see live sensor module readings, you will need to have a local database properly configured on your machine.

#### Steps
1. Follow the previous steps in the Sensor Module -> Base Station PC setup instructions if you want to run this at the same time as the sensor module.
2. Plug in one of the WiFi adapters to the Raspberry Pi with the camera, which is powered via GPIO pins and a power supply (or some other stable means).
3. Plug in the other WiFi adapter to your machine.
4. Start the OpenHD ground software. Tip: If you have QOpenHD installed as well, you can use this to validate that your base station PC is properly receiving frames from the Raspberry Pi. It may take a minute or two for the Pi to boot up properly and start streaming.
5. Navigate to `models/` and run `python3 stream_and_classify.py`. This will print out model classifications to the terminal.