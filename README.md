# CubeSat Mission Design: Store-and-Forward Image System (v1.0)

## 1. System Overview
The mission involves an integrated system for satellite communication (Uplink/Downlink), image capture (Arducam Mega), and persistent storage (SD Card). The firmware is designed to be modular, using the original project files as callable modules within the Arduino IDE.

## 2. Communication Protocol (Uplink)
The CubeSat listens for FSK radio packets at 435MHz (9.6kbps).
- **`PING`**: Request a health check. Returns `PONG`.
- **`CAPT`**: Trigger camera, save image to SD card with a unique ID.
- **`REQP:<ID>`**: Request a specific image by its ID.

## 3. Storage Logic (Store-and-Forward)
- Images are saved as `IMG_#.JPG` on the SD card using the `SdFat` library.
- A `config.txt` file tracks the `nextImageID` to prevent overwriting after a reboot.

## 4. Packetization Logic (Downlink) 
Images are broken into **120-byte data chunks** to ensure high reliability and efficient radio buffer usage.

### Packet Structure (128 bytes total)
| Field | Size | Description |
| :--- | :--- | :--- |
| **Header** | **8 bytes** | Metadata for reconstruction |
| - Image ID | 2 bytes | uint16_t |
| - Packet Index | 2 bytes | uint16_t (1-based) |
| - Total Packets | 2 bytes | uint16_t |
| - Data Size | 1 byte | uint8_t (bytes of JPEG data) |
| - Checksum | 1 byte | XOR checksum of the data portion |
| **Data** | **120 bytes** | Raw JPEG bytes |

## 5. Ground Station Logic
1. Receive 128-byte packets.
2. Verify checksum for the 120-byte data payload.
3. Use `Packet Index` to place data in a local buffer.
4. Once all packets are received (`Packet Index == Total Packets`), save the buffer as a `.jpg` file.
5. Send acknowledgement or request missing packets if necessary (future enhancement).

## 6. Project Architecture (Hardware Abstraction Layer)
This firmware uses a professional HAL-based architecture to separate hardware details from mission logic.

- **`config.h`**: **The Map.** Centralized configuration file containing all pin definitions (SPI, CS), mission constants (Frequency, Bitrate), and protocol structures.
- **`mission_integrated.ino`**: **The Orchestrator.** High-level mission controller that manages the state machine and coordinates between modules without needing to know hardware pin details.
- **`commu_tx_rx.ino`**: **Communication Module.** Owns the SX1278 radio object and provides a simple interface for uplink commands and downlink packet transmission.
- **`obc_camera2sd.ino`**: **Camera Module.** Manages the Arducam Mega hardware, handling image capture and raw JPEG extraction from the camera buffer.
- **`obc_sd_card.ino`**: **Storage Module.** Manages the SD Card via SdFat, providing persistent storage for images and tracking the Mission Image ID across reboots.

## 7. Setup & Deployment
1. Open `mission_integrated.ino` in the Arduino IDE.
2. Ensure the other four files (`config.h`, `commu_tx_rx.ino`, `obc_camera2sd.ino`, `obc_sd_card.ino`) are in the same folder.
3. Verify that the pins in `config.h` match your physical wiring.
4. Compile and upload to the STM32 OBC.

