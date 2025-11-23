# Auto Bitrate Feature Audit

## Findings
- **Critical – auto bitrate logic never executes on Sunshine clients (moonlight-common-c/src/ControlStream.c:1536-1591, apollo/src/stream.cpp:898-1006)**: The Moonlight control thread switches to the `usePeriodicPing` path for APP_VERSION >= 7.1.415 and Sunshine targets, where it only sends periodic pings and optional per-frame FEC status. It never emits the IDX_LOSS_STATS packet the host listens for, so Apollo’s auto-bitrate controller (adjustments, encoder reconfig mail, and stats publishing) is never triggered and the client overlay never receives stats. The feature is effectively inert on current clients.
- **Moderate – connection status state persists across sessions (moonlight-common-c/src/ControlStream.c:113,362-420)**: `lastSentConnectionStatus` is a static that isn’t reset in `initializeControlStream()`. If the prior session ended in the same status as the new one (commonly OKAY), the client suppresses sending the first connection-status message to the host. With loss stats already absent, this can leave Apollo without any auto-bitrate signal for the entire session.

## Recommendations
1) Reintroduce loss stats (or an equivalent trigger) on the periodic-ping path and/or call the host auto-bitrate controller from another heartbeat so adjustments and stats work on modern clients.
2) Reset `lastSentConnectionStatus` during control-stream initialization to ensure each session starts with a fresh status emission.
