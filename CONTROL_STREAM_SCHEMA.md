# Control Stream Schema Contract

This document defines the schema contract between the host (Apollo/Sunshine) and client (Moonlight) for control stream messages related to auto bitrate functionality.

## General Protocol Information

- **Endianness**: All multi-byte values are little-endian
- **Transport**: TCP control stream (encrypted for Gen7+)
- **Packet Format**: All packets include a `control_header_v2` header followed by payload data
- **Packet Types**: Defined by protocol generation (Gen3, Gen4, Gen5, Gen7, Gen7Enc)

## Message Types

### IDX_LOSS_STATS (0x0201)

**Direction**: Client → Host  
**Packet Type Index**: 3  
**Payload Size**: 32 bytes (fixed)  
**Frequency**: Every 50ms (`LOSS_REPORT_INTERVAL_MS`)

#### Payload Structure

| Offset | Size | Type | Field Name | Description |
|--------|------|------|------------|-------------|
| 0-3 | 4 | int32_t | count | Frame loss count (always 0, not computed by client) |
| 4-7 | 4 | int32_t | time_interval_ms | Time interval since last report in milliseconds (typically 50) |
| 8-11 | 4 | int32_t | unused_1 | Reserved field (value: 1000) |
| 12-19 | 8 | int64_t | lastGoodFrame | Last successfully received frame number (written as 64-bit, but value is uint32_t) |
| 20-23 | 4 | int32_t | unused_2 | Reserved field (value: 0) |
| 24-27 | 4 | int32_t | unused_3 | Reserved field (value: 0) |
| 28-31 | 4 | int32_t | unused_4 | Reserved field (value: 0x14) |

#### Client Implementation

```c
// Client constructs payload (moonlight-common-c/src/ControlStream.c)
BbPut32(&byteBuffer, 0);                    // count (always 0)
BbPut32(&byteBuffer, LOSS_REPORT_INTERVAL_MS); // time_interval_ms (50)
BbPut32(&byteBuffer, 1000);                 // unused_1
BbPut64(&byteBuffer, lastGoodFrame);         // lastGoodFrame (uint32_t written as 64-bit)
BbPut32(&byteBuffer, 0);                    // unused_2
BbPut32(&byteBuffer, 0);                    // unused_3
BbPut32(&byteBuffer, 0x14);                 // unused_4
```

#### Host Implementation

```cpp
// Host parses payload (apollo/src/stream.cpp)
int32_t *stats = (int32_t *) payload.data();
int32_t count = stats[0];                    // Offset 0
std::chrono::milliseconds t {stats[1]};      // Offset 4

// Read lastGoodFrame as 64-bit (offset 12)
int64_t lastGoodFrame64;
std::memcpy(&lastGoodFrame64, &stats[3], sizeof(int64_t));
uint64_t lastGoodFrame = static_cast<uint64_t>(lastGoodFrame64);
```

#### Important Notes

1. **Loss Count**: The `count` field is always 0 from the client. The host must compute frame loss by tracking `lastGoodFrame` progression over time.

2. **Frame Loss Calculation**: The host calculates frame loss as:
   ```
   expectedFrames = framerate × (time_interval_ms / 1000.0)
   expectedCurrentFrame = last_reported_good_frame + expectedFrames
   lossCount = max(0, expectedCurrentFrame - lastGoodFrame)
   frameLossPercent = (lossCount / expectedFrames) × 100
   ```

3. **Alignment**: The 64-bit `lastGoodFrame` at offset 12 may not be 8-byte aligned. Use `memcpy` to safely read it.

4. **Validation**: The host should validate payload size is exactly 32 bytes before parsing.

---

### IDX_CONNECTION_STATUS (0x3003)

**Direction**: Bidirectional (Host ↔ Client)  
**Packet Type Index**: 19  
**Payload Size**: 1 byte (after header)  
**Protocol**: Apollo protocol extension (Gen7+ only)

#### Payload Structure

| Offset | Size | Type | Field Name | Description |
|--------|------|------|------------|-------------|
| 0 | 1 | uint8_t | status | Connection status: 0 = OKAY, 1 = POOR |

#### Status Values

- **0 (CONN_STATUS_OKAY)**: Network conditions are acceptable
- **1 (CONN_STATUS_POOR)**: Network conditions are poor, bitrate reduction recommended

#### Host → Client

**Purpose**: Host informs client of network quality assessment based on frame loss metrics.

**Host Implementation**:
```cpp
// Host sends to client (apollo/src/stream.cpp)
control_connection_status_t plaintext {};
plaintext.header.type = packetTypes[IDX_CONNECTION_STATUS];
plaintext.header.payloadLength = sizeof(control_connection_status_t) - sizeof(control_header_v2);
plaintext.status = (newStatus == session_t::connection_status_e::POOR) ? 1 : 0;
```

**Client Implementation**:
```c
// Client receives from host (moonlight-common-c/src/ControlStream.c)
uint8_t statusByte;
BbGet8(&bb, &statusByte);
hostConnectionStatus = (statusByte == 1) ? CONN_STATUS_POOR : CONN_STATUS_OKAY;
```

#### Client → Host

**Purpose**: Client reports its own network quality assessment as a fallback when loss stats cannot be sent reliably.

**Client Implementation**:
```c
// Client sends to host (via ListenerCallbacks.connectionStatusUpdate)
// The callback implementation should send:
// status = 0 for CONN_STATUS_OKAY
// status = 1 for CONN_STATUS_POOR
```

**Host Implementation**:
```cpp
// Host receives from client (apollo/src/stream.cpp)
std::uint8_t clientStatus = payload[0];
bool clientReportsPoor = (clientStatus == 1);
```

#### Important Notes

1. **Fallback Mechanism**: Client-to-host connection status is used as a fallback when loss stats are not being received reliably (e.g., control stream timeouts).

2. **Host Behavior**: When host receives POOR status from client, it treats it as 10% frame loss to trigger immediate bitrate reduction.

3. **Validation**: The host should validate payload size is at least 1 byte before reading the status.

---

## Control Header Structure

All control messages include a `control_header_v2` header:

```cpp
struct control_header_v2 {
  std::uint16_t type;        // Packet type (e.g., 0x0201 for IDX_LOSS_STATS)
  std::uint16_t payloadLength; // Payload length in bytes
};
```

**Note**: For encrypted control streams (Gen7+), the payload is encrypted and includes additional encryption metadata.

---

## Protocol Generation Differences

### Loss Stats Payload Size

- **Gen3/Gen4**: 32 bytes
- **Gen5**: 32 bytes
- **Gen7/Gen7Enc**: 32 bytes

### Connection Status Support

- **Gen3/Gen4/Gen5**: Not supported (payload length = -1)
- **Gen7/Gen7Enc**: Supported (packet type 0x3003)

---

## Error Handling

### Loss Stats

- **Invalid Payload Size**: Host should log error and ignore packet if size < 32 bytes
- **Zero Time Interval**: Host should skip frame loss calculation if `time_interval_ms == 0`
- **Negative Loss Count**: Host should clamp to 0 (may occur due to data corruption)

### Connection Status

- **Invalid Payload Size**: Host should log error and ignore packet if size < 1 byte
- **Unknown Status Value**: Host should treat any non-zero value as POOR for safety

---

## Frame Loss Computation Details

Since the client sends `count = 0`, the host must compute frame loss server-side:

1. **Track Last Good Frame**: Store `lastGoodFrame` from each loss stats report
2. **Calculate Expected Progression**: Based on framerate and time elapsed
3. **Compute Loss**: Difference between expected and actual frame progression
4. **Update Metrics**: Feed computed loss percentage to `AutoBitrateController`

### Example Calculation

```
Given:
- Framerate: 120 fps
- Time interval: 50 ms
- Last reported frame: 1000
- Current lastGoodFrame: 1005

Expected frames: 120 × (50 / 1000) = 6 frames
Expected current frame: 1000 + 6 = 1006
Actual current frame: 1005
Loss count: 1006 - 1005 = 1 frame
Frame loss %: (1 / 6) × 100 = 16.67%
```

---

## References

- **Client Implementation**: `moonlight-common-c/src/ControlStream.c`
- **Host Implementation**: `apollo/src/stream.cpp`
- **Auto Bitrate Controller**: `apollo/src/auto_bitrate.cpp`
- **Session Structure**: `apollo/src/stream.h`

---

## Version History

- **2025-01-XX**: Initial schema documentation
  - Documented IDX_LOSS_STATS payload structure
  - Documented IDX_CONNECTION_STATUS payload structure
  - Fixed host-side parsing of 64-bit lastGoodFrame field
  - Added frame loss computation details

