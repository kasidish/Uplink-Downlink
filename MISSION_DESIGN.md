# CubeSat Mission Design: Store-and-Forward Image System

## 1. System Overview
The mission involves an integrated system for satellite communication (Uplink/Downlink), image capture (Arducam), and persistent storage (SD Card).

## 2. Communication Protocol (Uplink)
The CubeSat listens for FSK radio packets at 435MHz (9.6kbps).
- **`CAPTURE`**: Trigger camera, save image to SD card with a unique ID.
- **`REQ:<ID>`**: Request a specific image by its ID.

## 3. Storage Logic (Store-and-Forward)
- Images are saved as `IMG_#.JPG` on the SD card.
- A `config.txt` file tracks the `nextImageID` to prevent overwriting after a reboot.

## 4. Packetization Logic (Downlink)
Images are too large for a single transmission and are broken into 240-byte chunks.

### Packet Structure (250 bytes total)
| Field | Size | Description |
| :--- | :--- | :--- |
| **Header** | 10 bytes | Metadata for reconstruction |
| - Image ID | 2 bytes | ID of the image being sent |
| - Packet Index | 2 bytes | Current chunk number |
| - Total Packets | 2 bytes | Total chunks for this image |
| - Data Length | 1 byte | Bytes of JPEG data in this packet |
| - Checksum | 3 bytes | Error detection |
| **Data** | 240 bytes | Raw JPEG bytes |

## 5. Ground Station Logic
1. Receive packets.
2. Verify checksum.
3. Use `Packet Index` to place data in a buffer.
4. Once `Packet Index == Total Packets`, save the buffer as a `.jpg` file.
