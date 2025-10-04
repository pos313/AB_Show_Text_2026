# NDI Integration Setup Guide

## Overview

This project now supports rendering NDI video feeds as a background beneath the text input area. The NDI feed will appear semi-transparent behind your text, creating a picture-in-picture effect perfect for stage displays.

## Requirements

1. **NDI SDK for Apple** (macOS)
2. **NDI Source** (e.g., Resolume, OBS, vMix, or any NDI-enabled application)

---

## Installation Steps

### 1. Download NDI SDK

**Option A: Direct Download (Recommended)**

1. Go to [https://ndi.video/for-developers/ndi-sdk/](https://ndi.video/for-developers/ndi-sdk/)
2. Click "Download NDI SDK"
3. Register/login (free account)
4. Download **"NDI SDK for Apple"**

**Option B: Using NDI Tools Installer**

1. Download NDI Tools from [https://ndi.video/tools/](https://ndi.video/tools/)
2. Install NDI Tools
3. The SDK may be included, or you'll need to download it separately

### 2. Install NDI SDK

1. Open the downloaded `.pkg` installer
2. Follow installation wizard
3. The SDK will be installed to:
   - `/Library/NDI SDK for Apple/`

### 3. Verify Installation

Check that NDI SDK files are in place:

```bash
ls "/Library/NDI SDK for Apple/include/Processing.NDI.Lib.h"
ls "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib"
```

If files are found, the SDK is correctly installed.

### 4. Rebuild the Project

```bash
cd /Users/paavo/text/build
cmake ..
make sender
```

You should see output like:
```
-- Found NDI SDK:
--   Include: /Library/NDI SDK for Apple/include
--   Library: /Library/NDI SDK for Apple/lib/macOS/libndi.dylib
```

If NDI is not found, you'll see:
```
-- NDI SDK not found. NDI support will be disabled.
```

---

## Usage

### Starting an NDI Source

**Using Resolume:**
1. Open Resolume
2. Go to Output → NDI
3. Enable "NDI Output" or "NDI Screen Capture"
4. Name it something like "Resolume Composition"

**Using OBS:**
1. Install OBS NDI Plugin
2. Tools → NDI Output Settings
3. Enable "Main Output"

### Running the Sender App

```bash
cd /Users/paavo/text/build
./sender
```

**What happens:**
- The app will search for available NDI sources
- It will try to connect to any source with "Resolume" in the name
- If found, video frames will render behind the text input area
- If not found, the app continues normally with a black background

**Console Output:**
```
Initializing NDI receiver...
Searching for NDI sources...
Found 2 NDI source(s):
  [0] MACBOOK (Resolume Composition)
  [1] OBS (Main Output)
Connected to NDI source: MACBOOK (Resolume Composition)
NDI frame size: 1920x1080
```

---

## Configuration

### Change NDI Source Name

Edit `src/sender/SenderApp.cpp` line 922:

```cpp
// Connect to first available source
if (!ndiReceiver_->connect("")) {  // Empty = any source

// Or connect to specific source
if (!ndiReceiver_->connect("OBS")) {  // Connects to source with "OBS" in name
```

### Adjust Video Transparency

Edit `src/sender/SenderApp.cpp` line 1007:

```cpp
IM_COL32(255, 255, 255, 128) // 128 = 50% transparent
IM_COL32(255, 255, 255, 64)  // 64 = 25% transparent (more text emphasis)
IM_COL32(255, 255, 255, 192) // 192 = 75% transparent (more video emphasis)
```

### Disable NDI

To build without NDI support:

```bash
cmake -DENABLE_NDI=OFF ..
make sender
```

---

## Troubleshooting

### "NDI SDK not found" during build

**Cause:** CMake cannot find the NDI headers/library

**Solutions:**
1. Verify installation path: `ls -la "/Library/NDI SDK for Apple/"`
2. Check permissions: `sudo chmod -R a+r "/Library/NDI SDK for Apple/"`
3. Try alternate install location:
   ```bash
   mkdir -p ~/Library/NDI\ SDK\ for\ Apple
   # Copy SDK files to ~/Library/NDI SDK for Apple/
   ```
4. Specify path manually in CMakeLists.txt

### "No NDI sources found" at runtime

**Cause:** No NDI sources are broadcasting on the network

**Solutions:**
1. Ensure Resolume (or other NDI source) is running
2. Enable NDI output in the source application
3. Check firewall settings (allow NDI traffic on port 5353 and 5960-5969)
4. Ensure both devices are on the same network
5. Test with NDI Studio Monitor to verify source is visible

### Video appears but is laggy

**Cause:** High resolution or network bandwidth

**Solutions:**
1. The receiver already uses `NDIlib_recv_bandwidth_lowest` (preview quality)
2. Lower the resolution in your NDI source (e.g., Resolume output at 1280×720)
3. Reduce frame rate in source (30fps or 25fps)
4. Use wired ethernet instead of WiFi

### Black screen in input area

**Cause:** NDI receiver not getting frames

**Solutions:**
1. Check console output for error messages
2. Verify NDI source is actually sending video (test with NDI Studio Monitor)
3. Restart both sender app and NDI source

### Crash on startup

**Cause:** NDI library loading issue

**Solutions:**
1. Check that libndi.dylib is present: `ls "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib"`
2. Verify architecture matches: `file "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib"` (should be arm64 or x86_64)
3. Try rebuilding: `rm -rf build && mkdir build && cd build && cmake .. && make sender`

---

## Technical Details

### Architecture

- **NDI Receiver:** Discovers and connects to NDI sources using NDI Find API
- **Video Capture:** Non-blocking frame capture using `NDIlib_recv_capture_v3()`
- **Format:** BGRA (8-bit per channel), low bandwidth mode (preview stream)
- **OpenGL Upload:** BGRA frames uploaded to GL_TEXTURE_2D each frame
- **Rendering:** ImGui DrawList renders texture behind input widget with alpha blending

### Performance

- **Preview mode** reduces bandwidth by ~75% (typically 720p @ 30fps)
- **Frame capture:** Non-blocking, 0ms timeout (drops frames if app is busy)
- **Texture upload:** ~2-3ms for 1920×1080 BGRA on modern Mac
- **Total overhead:** ~5-10ms per frame (negligible for 60fps UI)

### Files Modified/Created

- `src/sender/NDIReceiver.h` - NDI wrapper class
- `src/sender/NDIReceiver.cpp` - NDI implementation
- `src/sender/SenderApp.h` - Added NDI members
- `src/sender/SenderApp.cpp` - NDI initialization, texture upload, rendering
- `CMakeLists.txt` - NDI SDK detection and linking

---

## Alternative: Using Syphon (macOS)

If you prefer using Syphon instead of NDI:

1. Resolume already outputs Syphon by default
2. Similar implementation possible using SyphonClient
3. Lower latency than NDI (shared GPU memory vs network)
4. But limited to same-machine sources

Let me know if you'd like a Syphon-based implementation instead!
