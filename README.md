# UpWork

![Group 6](https://user-images.githubusercontent.com/92107731/216580106-8868d6ee-ad57-4bcd-a55c-b629d2d95334.png)

This project is a school project that aims to use an ESP32 board, a fingerprint sensor, and an LCD screen to track the time of arrival and departure of employees. The system works by capturing the employee's fingerprint and sending the data to an MQTT broker. The broker then receives and processes the data to update the employee's attendance records.

## Requirements

 - ESP32 board
 - Fingerprint sensor
 - LCD screen
 - MQTT broker

## Usage

 1. Connect the ESP32 board, fingerprint sensor, and LCD screen as per the circuit diagram.
 2. Upload the code to the ESP32 board.
 3. Connect to the MQTT broker and start recording the attendance of employees by scanning their fingerprints.
 4. The attendance data can be monitored and managed from the MQTT broker.

## Wiring diagram

![Frame 43](https://user-images.githubusercontent.com/92107731/216539933-ebe31b1b-1e2b-46ee-9774-5ed7f074714b.png)

## Repositories for MQTT Broker

 - https://github.com/cdudit/upwork-web
 - https://github.com/cdudit/upwork-web-mqtt

## App iOS

This project also includes a mobile application for iOS devices that connects to a web server on a Raspberry Pi. The application retrieves the attendance data of employees from the server and displays the worked hours. The web server on the Raspberry Pi communicates with the ESP32 board through the MQTT broker to receive the latest attendance data.

The mobile application provides a convenient way for managers to monitor the attendance records of employees in real-time. The user-friendly interface allows for easy navigation and provides a clear overview of the attendance data.

https://github.com/gatienddr/upwork


