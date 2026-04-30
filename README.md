# CubeSat Mission Design: Store-and-Forward Image System (v2.0)

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

## 6. Project Structure
- `mission_integrated.ino`: Main controller and mission state machine.
- `commu_tx_rx.ino`: Radio communication module.
- `obc_camera2sd.ino`: Camera capture logic.
- `obc_sd_card.ino`: SD card management logic.
