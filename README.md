# CubeSat Store-and-Forward Image System

This project integrates three former standalone Arduino sketches into one CubeSat-style firmware system:

- SD card storage
- Arducam Mega image capture
- SX1278 FSK uplink/downlink communication

The goal is a Store-and-Forward Image System: the satellite receives a command, captures an image, stores it on the SD card, and later downlinks that image in small radio packets when requested.

## Current Status

Implemented command set:

| Command | Direction | Result |
| :--- | :--- | :--- |
| `PING` | Ground -> OBC | OBC replies `PONG` |
| `CAPT` | Ground -> OBC | OBC captures a JPEG image and stores it on SD |
| `REQP:<ID>` | Ground -> OBC | OBC downlinks image packets for the requested image ID |

Implemented storage format:

| Item | Format |
| :--- | :--- |
| Image files | `IMG_0001.JPG`, `IMG_0002.JPG`, ... |
| Persistent counter | `config.txt` |
| Counter content | `nextImageID=<number>` |

Implemented radio format:

| Item | Value |
| :--- | :--- |
| Radio | SX1278 |
| Mode | FSK |
| Frequency | 435.0 MHz |
| Bitrate | 9.6 kbps |
| Frequency deviation | 4.8 kHz |
| Packet size | 128 bytes |
| Header size | 8 bytes |
| JPEG payload size | 120 bytes |

## Repository Files

| File | Purpose |
| :--- | :--- |
| `mission_integrated.ino` | Main mission controller. Receives commands and calls the storage, camera, and radio modules. |
| `config.h` | Central pin map, radio settings, timing settings, packet constants, and packet header structure. |
| `commu_tx_rx.ino` | SX1278 communication module. Handles radio initialization, command receive, and packet transmit. |
| `obc_camera2sd.ino` | Camera module. Captures JPEG data from Arducam Mega and writes it to SD. |
| `obc_sd_card.ino` | SD module. Initializes SD card, saves/loads `nextImageID`, checks image existence, and opens image files. |

Arduino IDE compiles all `.ino` files in the same folder as one sketch, so these files act like modules even though they are not separate C++ libraries yet.

## System Workflow

### 1. Boot Workflow

```text
Power on / reset
  -> start Serial
  -> initialize SD card
  -> load nextImageID from config.txt
  -> initialize Arducam Mega
  -> initialize SX1278 radio in FSK mode
  -> enter command listening loop
```

If `config.txt` does not exist, the firmware creates it and starts from image ID `1`.

### 2. Idle/Uplink Workflow

```text
loop()
  -> listen for radio command
  -> if no command, continue listening
  -> if command received, trim and uppercase it
  -> dispatch command
```

Supported commands are intentionally simple ASCII strings so the ground station can send them easily.

### 3. Health Check Workflow: `PING`

```text
Ground sends: PING
OBC receives command
OBC sends: PONG
OBC returns to receive mode
```

Use this first during testing to confirm the radio path works before testing camera or SD behavior.

### 4. Capture Workflow: `CAPT`

```text
Ground sends: CAPT
OBC reads current nextImageID
OBC creates filename IMG_0001.JPG
OBC triggers Arducam Mega capture
OBC scans camera bytes for JPEG SOI marker: FF D8
OBC writes JPEG bytes to SD card
OBC stops when JPEG EOI marker is found: FF D9
OBC increments nextImageID
OBC updates config.txt
OBC sends: CAPT_OK:<ID>
```

If capture fails or a valid JPEG is not found before timeout, the incomplete file is removed and the OBC sends:

```text
ERR:CAPT_FAILED
```

### 5. Downlink Workflow: `REQP:<ID>`

Example:

```text
Ground sends: REQP:1
```

OBC behavior:

```text
Check that ID is valid
Check that IMG_0001.JPG exists
Open image file
Calculate total packet count
Read 120 bytes from image
Build 8-byte packet header
Append JPEG payload
Transmit 128-byte packet
Repeat until the whole image is sent
Send EOF:<ID>
Return to receive mode
```

If the image does not exist, the OBC sends:

```text
ERR:NOT_FOUND:<ID>
```

## Downlink Packet Format

Each image packet is exactly 128 bytes.

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| Image ID | 2 bytes | `uint16_t` | Image number, for example `1` for `IMG_0001.JPG` |
| Packet Index | 2 bytes | `uint16_t` | Packet number, starting from `1` |
| Total Packets | 2 bytes | `uint16_t` | Total number of packets for this image |
| Data Size | 1 byte | `uint8_t` | Actual JPEG bytes in this packet, normally `120` except final packet |
| Checksum | 1 byte | `uint8_t` | XOR checksum of the JPEG payload bytes |
| JPEG Data | 120 bytes | `uint8_t[]` | Raw JPEG data, zero-padded after `Data Size` if needed |

The final packet is still transmitted as 128 bytes. Its `Data Size` field tells the ground station how many payload bytes are real image data.

## Ground Station Reconstruction Workflow

The ground station should do this:

```text
Receive 128-byte packet
Parse first 8 bytes as header
Read Data Size
Calculate XOR checksum over payload[0:Data Size]
Compare calculated checksum with header checksum
Store payload bytes at Packet Index position
Repeat until all packets from 1 to Total Packets are received
Write reconstructed bytes to IMG_<ID>.JPG
Stop when EOF:<ID> is received or when all packets are complete
```

Minimum receiver behavior:

1. Ignore packets with bad checksum.
2. Store packets by `Packet Index`, not by receive order.
3. Use `Data Size` for the last packet so zero padding is not written into the JPEG.
4. Rebuild the file only when every packet index from `1` to `Total Packets` exists.

Future improvement: add `REQM:<ID>:<packetIndex>` so the ground station can request missing packets again.

## Comparison With Former Code

### Former `obc_sd_card.ino`

| Former code | Integrated project |
| :--- | :--- |
| Standalone SD card test sketch. | Reused as a storage module. |
| Writes and reads `Hello.txt`. | Manages mission image files and `config.txt`. |
| Runs once inside `setup()`. | Provides callable functions used by mission logic. |
| Calls `sd.end()` after test. | Keeps SD card active for capture and downlink. |
| Uses local `file` object globally. | Opens files only when needed and returns image file handles. |

The former SD code proved the SD wiring and SdFat setup. The new code turns that test into mission storage logic.

### Former `obc_camera2sd.ino`

| Former code | Integrated project |
| :--- | :--- |
| Captures 3 images automatically in `loop()`. | Captures only when `CAPT` command is received. |
| File names are `0.jpg`, `1.jpg`, etc. | File names are mission IDs: `IMG_0001.JPG`, `IMG_0002.JPG`, etc. |
| Has a typo: `imageData = imageDataNext;a`. | Typo removed. |
| Uses blocking `while(1)` on file open failure. | Returns `false` so mission code can send an error. |
| Stops SD after capture test. | Keeps SD available for later image request. |
| No persistent image counter. | Uses `config.txt` so IDs survive reboot. |

The former camera code contained the important JPEG extraction logic: find `FF D8`, write bytes, stop at `FF D9`. The integrated version keeps that idea but makes it command-driven and safer.

### Former `commu_tx_rx.ino`

| Former code | Integrated project |
| :--- | :--- |
| Compile-time mode: either `TRANSMIT` or `RECEIVE`. | Runtime system can receive commands and transmit responses/packets. |
| Sends `Hello World! #n` in transmit mode. | Sends protocol replies and binary image packets. |
| Receive mode only prints received string. | Received strings control mission behavior. |
| Radio settings are hardcoded in the sketch. | Radio settings live in `config.h`. |
| No image packet protocol. | Implements 128-byte packet downlink format. |

The former communication code proved the SX1278 radio settings. The new code keeps the same FSK frequency, bitrate, frequency deviation, sync word, and output power, then adds mission command handling.

## Important Design Changes

| Area | Change | Reason |
| :--- | :--- | :--- |
| File naming | `IMG_0001.JPG` instead of `0.jpg` | Easier ground-station tracking and less ambiguity. |
| Persistent ID | `config.txt` stores `nextImageID` | Prevents overwriting images after reboot. |
| Packet size | Always sends 128-byte image packets | Predictable ground-station parser. |
| Error handling | Functions return `true`/`false` | Main mission logic can send useful radio errors. |
| Central config | Pins/constants moved to `config.h` | Easier hardware changes without editing mission logic. |
| Capture trigger | Radio command instead of automatic loop | Matches real uplink-controlled mission behavior. |

## Recommended Test Order

1. Compile the sketch in Arduino IDE.
2. Confirm pin definitions in `config.h` match your board wiring.
3. Upload to STM32 OBC.
4. Open Serial Monitor at `115200` baud.
5. Send `PING` from the ground radio and confirm `PONG`.
6. Send `CAPT` and confirm a file like `IMG_0001.JPG` appears on the SD card.
7. Send `REQP:1` and verify 128-byte packets are received by the ground station.
8. Reboot the OBC and send another `CAPT`; confirm it creates the next image ID instead of overwriting the old file.

## Known Limitations

- No missing-packet retransmission yet.
- No full ground-station decoder is included in this repository yet.
- Packet header uses the MCU's native byte order, so the ground station should parse little-endian values if it runs on a PC.
- `arduino-cli` was not available on the development machine, so compile verification still needs to be done in Arduino IDE or with Arduino CLI installed.

## Future Improvements

Recommended next features:

1. Add `LIST` command to downlink available image IDs.
2. Add `STAT` command for SD/radio/camera health status.
3. Add `REQM:<ID>:<packetIndex>` for missing packet retransmission.
4. Add a ground-station Python decoder for rebuilding JPEGs.
5. Add CRC-16 instead of XOR checksum for stronger packet validation.
6. Add image size and capture timestamp metadata.
