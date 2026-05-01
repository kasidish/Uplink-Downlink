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
| `STAT` | Ground -> OBC | OBC reports SD/camera/radio status and packet sizing |
| `LIST` | Ground -> OBC | OBC lists stored image IDs |
| `CAPT` | Ground -> OBC | OBC captures a JPEG image and stores it on SD |
| `REQP:<IMAGE_ID>` | Ground -> OBC | OBC downlinks all image chunks for the requested image ID |
| `REQM:<IMAGE_ID>:<CHUNK_ID>` | Ground -> OBC | OBC retransmits one missing image chunk |
| `DELI:<IMAGE_ID>` | Ground -> OBC | OBC deletes the requested image from SD |

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
| Link frame | Plain SX1278 FSK packet by default; optional AX.25 UI-style wrapper |
| Mission packet size | 128 bytes |
| Mission header size | 9 bytes |
| JPEG payload size | 119 bytes |
| Payload checksum | CRC-16 |

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
  -> dispatch command by command type and IDs
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

### 5. Status Workflow: `STAT`

```text
Ground sends: STAT
OBC sends: STAT:SD=1,CAM=1,RADIO=1,NEXT_IMAGE_ID=51,PKT_TOTAL=128,PKT_DATA=119
```

Use this before capture or downlink to confirm that the core subsystems initialized.

### 6. Image List Workflow: `LIST`

```text
Ground sends: LIST
OBC sends: LIST:1,2,3,10,50
OBC sends: LIST:END
```

If no stored image exists, the OBC sends:

```text
LIST:EMPTY
```

### 7. Downlink Workflow: `REQP:<IMAGE_ID>`

Example:

```text
Ground sends: REQP:1
```

OBC behavior:

```text
Check that ID is valid
Check that IMG_0001.JPG exists
Open image file
Calculate total chunk count
Read 119 bytes from image
Build 9-byte packet header
Append JPEG payload
Transmit 128-byte chunk
Repeat until the whole image is sent
Send EOF:<ID>
Return to receive mode
```

If the image does not exist, the OBC sends:

```text
ERR:NOT_FOUND:<ID>
```

### 8. Missing Chunk Workflow: `REQM:<IMAGE_ID>:<CHUNK_ID>`

Example:

```text
Ground sends: REQM:1:7
```

OBC behavior:

```text
Check that image ID and chunk ID are valid
Open IMG_0001.JPG
Seek to chunk 7 offset
Transmit only chunk 7 as a normal 128-byte mission packet
Return to receive mode
```

This command is for selective retry after the ground station detects a bad checksum or a missing chunk. It carries both identifiers:

| Identifier | Purpose |
| :--- | :--- |
| `IMAGE_ID` | Selects the source image file, for example `IMG_0001.JPG` |
| `CHUNK_ID` | Selects the 119-byte slice inside that image, starting from `1` |

### 9. Delete Image Workflow: `DELI:<IMAGE_ID>`

Example:

```text
Ground sends: DELI:1
```

OBC behavior:

```text
Check that image ID is valid
Delete IMG_0001.JPG from SD
Send DELI_OK:1 on success
Return to receive mode
```

The delete command only needs `IMAGE_ID`; it deletes the whole stored JPEG, not one chunk.

## Downlink Packet Format

Each image mission packet is exactly 128 bytes before it is wrapped in the AX.25 UI-style link frame.

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| Image ID | 2 bytes | `uint16_t` | Image number, for example `1` for `IMG_0001.JPG` |
| Chunk ID | 2 bytes | `uint16_t` | Chunk number, starting from `1` |
| Total Chunks | 2 bytes | `uint16_t` | Total number of chunks for this image |
| Checksum | 2 bytes | `uint16_t` | CRC-16 of the JPEG payload bytes |
| Data Size | 1 byte | `uint8_t` | Actual JPEG bytes in this packet, normally `119` except final packet |
| JPEG Data | 119 bytes | `uint8_t[]` | Raw JPEG data, zero-padded after `Data Size` if needed |

The final packet is still transmitted as 128 bytes. Its `Data Size` field tells the ground station how many payload bytes are real image data.

## Link Frame Format

By default, `USE_AX25_FRAME` is `false` in `config.h`. In this mode the radio sends plain command strings and 128-byte mission image packets directly through RadioLib SX1278 FSK packet mode. This is the recommended mode while testing through Arduino IDE Serial Monitor because `PING`, `STAT`, `LIST`, `CAPT`, and other replies remain easy to read.

If `USE_AX25_FRAME` is changed to `true`, the radio module sends the command/status text or mission packet as the AX.25 information field. The current implementation builds an AX.25 UI-style frame:

| Field | Value |
| :--- | :--- |
| Destination callsign | `GROUND-0` |
| Source callsign | `CUBSAT-0` |
| Control | `0x03` UI frame |
| PID | `0xF0` no layer 3 |
| Info | ASCII command/status text or 128-byte image mission packet |
| FCS | AX.25 16-bit FCS |

This is an AX.25 byte-frame envelope carried by RadioLib's SX1278 FSK packet mode. It is not yet a complete raw over-the-air AX.25 modem implementation with NRZI and bit stuffing.

Recommended development flow:

1. Keep `USE_AX25_FRAME=false` while testing with Arduino Serial Monitor.
2. Verify capture, list, delete, full image downlink, and missing-chunk retry.
3. Ask the mission/professor requirement for whether AX.25 framing is required.
4. If required, set `USE_AX25_FRAME=true` and add matching AX.25 unwrap/wrap logic to the ground station sketch.

## Ground Station Reconstruction Workflow

The ground station should do this:

```text
Receive AX.25 UI frame
Extract the information field
Receive 128-byte mission chunk
Parse first 9 bytes as header
Read Data Size
Calculate CRC-16 over payload[0:Data Size]
Compare calculated checksum with header checksum
Store payload bytes at Chunk ID position
Repeat until all chunks from 1 to Total Chunks are received
Write reconstructed bytes to IMG_<ID>.JPG
Stop when EOF:<ID> is received or when all chunks are complete
```

Minimum receiver behavior:

1. Ignore packets with bad checksum.
2. Store packets by `Chunk ID`, not by receive order.
3. Use `Data Size` for the last packet so zero padding is not written into the JPEG.
4. Rebuild the file only when every chunk from `1` to `Total Chunks` exists.
5. Send `REQM:<IMAGE_ID>:<CHUNK_ID>` for missing or corrupt chunks.

The current packet header stores `imageID` and `chunkID`, which is the minimum information needed for selective retransmission.

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
| No image packet protocol. | Implements 128-byte mission packet downlink format. |

The former communication code proved the SX1278 radio settings. The new code keeps the same FSK frequency, bitrate, frequency deviation, sync word, and output power, then adds mission command handling.

## Important Design Changes

| Area | Change | Reason |
| :--- | :--- | :--- |
| File naming | `IMG_0001.JPG` instead of `0.jpg` | Easier ground-station tracking and less ambiguity. |
| Persistent ID | `config.txt` stores `nextImageID` | Prevents overwriting images after reboot. |
| Packet size | Always sends 128-byte mission image chunks | Predictable ground-station parser. |
| Link frame | Wraps command/status/chunk data in AX.25 UI-style frames | Closer to CubeSat TT&C conventions. |
| Error handling | Functions return `true`/`false` | Main mission logic can send useful radio errors. |
| Central config | Pins/constants moved to `config.h` | Easier hardware changes without editing mission logic. |
| Capture trigger | Radio command instead of automatic loop | Matches real uplink-controlled mission behavior. |

## Recommended Test Order

1. Compile the sketch in Arduino IDE.
2. Confirm pin definitions in `config.h` match your board wiring.
3. Upload to STM32 OBC.
4. Open Serial Monitor at `115200` baud.
5. Send `PING` from the ground radio and confirm `PONG`.
6. Send `STAT` and confirm SD/camera/radio status.
7. Send `CAPT` and confirm a file like `IMG_0001.JPG` appears on the SD card.
8. Send `LIST` and confirm the new image ID is listed.
9. Send `REQP:1` and verify 128-byte mission chunks are received by the ground station.
10. Send `REQM:1:1` and verify chunk 1 can be retransmitted.
11. Send `DELI:1` only after confirming the delete path should remove that image.
12. Reboot the OBC and send another `CAPT`; confirm it creates the next image ID instead of overwriting the old file.

## Known Limitations

- No full ground-station decoder is included in this repository yet.
- Packet header uses the MCU's native byte order, so the ground station should parse little-endian values if it runs on a PC.
- AX.25 support is optional and disabled by default for easier Serial Monitor testing.
- AX.25 support is currently a UI-frame envelope inside RadioLib FSK packet mode, not a full AX.25 physical/link modem.
- `arduino-cli` was not available on the development machine, so compile verification still needs to be done in Arduino IDE or with Arduino CLI installed.

## Future Improvements

Recommended next features:

1. Add a ground-station Python decoder for AX.25 unwrap and JPEG rebuild.
2. Add image size and capture timestamp metadata.
3. Add full raw AX.25 modem behavior if the mission requires interoperability with standard packet-radio tools.
