# Investigation: IDX_LOSS_STATS Payload Size Discrepancy

## Issue Summary

A code review claims that `IDX_LOSS_STATS` packets are 16 bytes, but the current code expects 32 bytes. This investigation examines the discrepancy.

## Current Implementation

### Client Side (moonlight-common-c/src/ControlStream.c)

**Payload Construction (Lines 1565-1572):**
```c
BbInitializeWrappedBuffer(&byteBuffer, lossStatsPayload, 0, payloadLengths[IDX_LOSS_STATS], BYTE_ORDER_LITTLE);
BbPut32(&byteBuffer, 0);                    // count (4 bytes)
BbPut32(&byteBuffer, LOSS_REPORT_INTERVAL_MS); // time_interval_ms (4 bytes)
BbPut32(&byteBuffer, 1000);                 // unused_1 (4 bytes)
BbPut64(&byteBuffer, lastGoodFrame);         // lastGoodFrame (8 bytes)
BbPut32(&byteBuffer, 0);                    // unused_2 (4 bytes)
BbPut32(&byteBuffer, 0);                    // unused_3 (4 bytes)
BbPut32(&byteBuffer, 0x14);                 // unused_4 (4 bytes)
```

**Total bytes written:** 4 + 4 + 4 + 8 + 4 + 4 + 4 = **32 bytes**

**Payload Length Configuration:**
All protocol versions define `payloadLengths[IDX_LOSS_STATS] = 32`:
- Gen3: 32 bytes (line 290)
- Gen4: 32 bytes (line 298)
- Gen5: 32 bytes (line 306)
- Gen7: 32 bytes (line 314)
- Gen7Enc: 32 bytes (line 322)

**Packet Sending (Line 1575-1576):**
```c
sendMessageAndForget(packetTypes[IDX_LOSS_STATS],
                     payloadLengths[IDX_LOSS_STATS],  // = 32
                     lossStatsPayload,
                     ...);
```

The `sendMessageEnet` function (line 772) sets:
```c
packet->payloadLength = paylen;  // = 32
memcpy(&packet[1], payload, paylen);  // Copies 32 bytes
```

### Server Side (apollo/src/stream.cpp)

**Payload Validation (Lines 871-875):**
```cpp
if (payload.size() < 32) {
  BOOST_LOG(info) << "AutoBitrate: [LossStats] Invalid payload size: " 
                  << payload.size() << " bytes (expected: 32 bytes)";
  return;
}
```

**Payload Parsing (Lines 877-884):**
```cpp
int32_t *stats = (int32_t *) payload.data();
auto count = stats[0];                    // Offset 0-3 (4 bytes)
std::chrono::milliseconds t {stats[1]};  // Offset 4-7 (4 bytes)

// Extract lastGoodFrame (64-bit at offset 12)
int64_t lastGoodFrame64;
std::memcpy(&lastGoodFrame64, &stats[3], sizeof(int64_t));  // Offset 12-19 (8 bytes)
uint64_t lastGoodFrame = static_cast<uint64_t>(lastGoodFrame64);
```

**Minimum required bytes:** 4 + 4 + 8 = **16 bytes** (but handler reads 8 bytes starting at offset 12, so needs at least 20 bytes)

## Code Review Claim

The review states:
> "IDX_LOSS_STATS packets coming from Moonlight are 16-byte payloads (four int32 fields, which is exactly what the handler reads via stats[0..3])."

## Historical Evidence

From `DYNAMIC_BITRATE_IMPLEMENTATION_REPORT.md` (line 425), the **OLD** handler code was:
```cpp
auto lastGoodFrame = stats[3];  // Read as single int32_t
```

This suggests that originally:
- `stats[0]` = count (4 bytes)
- `stats[1]` = time interval (4 bytes)  
- `stats[2]` = unused (4 bytes)
- `stats[3]` = lastGoodFrame as int32 (4 bytes)

**Total: 16 bytes**

## The Discrepancy

1. **Client code writes:** 32 bytes (7 fields: 3×int32 + 1×int64 + 3×int32)
2. **Client config says:** 32 bytes (`payloadLengths[IDX_LOSS_STATS] = 32`)
3. **Server expects:** ≥32 bytes
4. **Review claims:** 16 bytes (4×int32)
5. **Old handler read:** `stats[3]` as int32 (suggesting 16-byte payload)

## Possible Explanations

### Theory 1: Protocol Version Mismatch
- Older Moonlight clients send 16-byte payloads
- Newer clients send 32-byte payloads
- Server needs to support both

### Theory 2: Byte Buffer Bug
- `BbPut64` might not be writing 8 bytes correctly
- Buffer might be truncated somewhere
- Payload might be getting cut off during encryption/transmission

### Theory 3: Handler Bug
- Handler was updated to read 64-bit `lastGoodFrame` but payload wasn't updated
- Should read `lastGoodFrame` as int32 from `stats[3]` if payload is 16 bytes

### Theory 4: Review Error
- Review might be based on outdated information
- Actual packets are 32 bytes as code suggests

## Impact

If packets are actually 16 bytes:
- ✅ Handler reads `stats[0]` and `stats[1]` correctly (8 bytes)
- ❌ Handler tries to read 8 bytes from `stats[3]` (offset 12-19) but only 4 bytes exist (offset 12-15)
- ❌ **Out-of-bounds read** - undefined behavior
- ❌ **All packets rejected** by size check, breaking auto-bitrate

## Recommended Actions

1. **Add runtime logging** to verify actual payload sizes received
2. **Check if there are multiple Moonlight client versions** with different payload sizes
3. **Make handler backward-compatible** to support both 16-byte and 32-byte payloads
4. **Verify byte buffer operations** ensure all 32 bytes are written

## Next Steps

1. Add debug logging: `BOOST_LOG(debug) << "LossStats payload size: " << payload.size();`
2. Check if `payloadLength` field in packet header matches actual payload size
3. Test with different Moonlight client versions
4. Consider making the size check more lenient: `if (payload.size() < 16)` instead of `if (payload.size() < 32)`

