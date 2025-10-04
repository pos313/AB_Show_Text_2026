# Live Text Rendering System

A robust dual-feed live text rendering system for stage performances, built with C++ and Aeron messaging.

## System Overview

This system consists of two applications:
- **Sender**: Stage laptop application with dark mode GUI, NDI video background, and text input
- **Receiver**: Front-of-house renderer that outputs text via Spout/Syphon to Resolume/video systems

### Key Features

- **NDI Video Background**: Real-time NDI video feed as background in text input area (sender)
- **Multi-line Text Support**: Full newline support with per-line centering in outputs
- **Dual Redundant Aeron Feeds**: Two independent UDP channels for reliability (localhost UDP)
- **Automatic Failover**: Seamlessly switches between feeds if one fails
- **Smooth Fade-out**: 2-second exponential fade when text is cleared
- **Dark Mode GUI**: Beautiful interface optimized for stage use
- **Text Memory**: Shows previously displayed messages (>3 seconds display time)
- **Health Monitoring**: Real-time connection status and system health
- **Spout/Syphon Integration**: Alpha-transparent 4K output for video systems
- **Dual Text Sizes**: Separate Syphon/Spout outputs for small and large text modes
- **Exclusive Output**: Inactive size output is always blank, preventing dual displays

## Architecture

### Communication Layer
- **Aeron Messaging**: Ultra-low latency UDP (localhost)
- **Primary Feed**: `aeron:udp?endpoint=127.0.0.1:9999`
- **Secondary Feed**: `aeron:udp?endpoint=127.0.0.1:9998`
- **Stream ID**: 1001
- **NDI Input**: Real-time NDI video feed for sender background

### Rendering
- **OpenGL**: Hardware-accelerated text rendering
- **FreeType**: High-quality font rendering with ABF custom font
- **Spout/Syphon**: Texture sharing for video integration
  - Windows: Spout (DirectX/OpenGL interop)
  - macOS: Syphon (native OpenGL sharing)
- **4K Output**: 3840x2160 resolution for both small and large text outputs
- **Dual Outputs**: Separate "LiveText-Small" and "LiveText-Big" Syphon/Spout servers
- **Alpha Blending**: Transparent background for overlay compositing
- **Multi-line Support**: Proper newline handling with per-line centering

## Building

### Dependencies

#### Required Libraries
- **Aeron**: High-performance messaging library (stubbed for local UDP)
- **GLFW**: OpenGL context and window management
- **OpenGL**: 3.3+ with gl3w loader
- **FreeType**: Font rendering
- **GLM**: OpenGL Mathematics
- **ImGui**: Immediate mode GUI
- **NDI SDK**: NewTek NDI for video input (sender only)

#### Platform-Specific
- **Windows**: Spout SDK for texture sharing
- **macOS**: Syphon SDK for texture sharing
- **Linux**: Development/testing mode (no texture sharing)

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make -j4

# Or on Windows
cmake --build . --config Release
```

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get install libglfw3-dev libfreetype6-dev libglm-dev
# Install Aeron from: https://github.com/aeron-io/aeron
```

#### macOS
```bash
brew install glfw freetype glm
# Install Aeron from: https://github.com/aeron-io/aeron
```

#### Windows
- Install Spout SDK
- Install vcpkg for dependencies:
```cmd
vcpkg install glfw3 freetype glm
```

## Usage

### Starting the System

1. **Start Media Driver** (if using external driver):
```bash
java -cp aeron-all.jar io.aeron.driver.MediaDriver
```

2. **Start Receiver** (Front of House):
```bash
./receiver
```

3. **Start Sender** (Stage Laptop):
```bash
./sender
```

### Sender Controls

- **Text Input**: Type text that will appear on screen
- **Small Text / Big Text**: Switch between font sizes
- **Send Text**: Manually send text (auto-send available)
- **Clear**: Trigger smooth fade-out
- **Auto-send**: Automatically send text as you type

### Text Memory
- Shows messages displayed for more than 3 seconds
- Latest message appears at the top
- Scrollable history of previous messages

### Health Monitoring

Both applications provide real-time health monitoring:
- **Connection Status**: Shows primary/secondary feed status
- **Message Counters**: Displays throughput statistics
- **Active Feed**: Indicates which feed is currently active
- **Error Reporting**: Shows connection errors and recovery status

### Sender Controls

- **Ctrl+Enter**: Send text
- **Ctrl+D**: Clear text and trigger fade-out
- **Ctrl+1**: Switch to small text mode
- **Ctrl+2**: Switch to big text mode
- **F1**: Toggle keyboard shortcuts help
- **Auto-send**: Automatically send text as you type
- **Input validation**: Real-time feedback on text length and validity

### Receiver Controls

- **ESC**: Exit application
- **H**: Print health status to console
- **S**: Print detailed subscriber statistics

## Network Configuration

### Multicast Setup

Ensure multicast is enabled on your network:

```bash
# Linux - add multicast routes
sudo route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0

# Check multicast groups
netstat -gn
```

### Firewall Configuration

Allow UDP traffic on ports 9999 for both feeds:
- Primary: 224.0.1.1:9999
- Secondary: 224.0.1.2:9999

### Performance Tuning

For optimal performance:
- Dedicate CPU cores to the applications
- Use high-priority/realtime scheduling
- Configure network buffers appropriately
- Ensure multicast-capable network infrastructure

## Video Integration

### Windows - Resolume with Spout
1. In Resolume, look for "LiveText" in Sources > Spout
2. Drag the Spout source to a composition layer
3. The text will appear with transparent background
4. Use blend modes for different visual effects

### macOS - Syphon Output
1. The receiver creates two Syphon servers:
   - **LiveText-Small**: Small text output (4K)
   - **LiveText-Big**: Large text output (4K)
2. Use any Syphon client (Resolume, MadMapper, VDMX, etc.) to receive the feeds
3. Only the active text size server shows content; inactive server is blank
4. Text appears with alpha transparency for overlay compositing
5. Multi-line text is supported with proper per-line centering

## Troubleshooting

### Common Issues

#### No Connection
- Check multicast routing
- Verify firewall settings
- Ensure both apps use same stream ID

#### Poor Performance
- Check CPU usage and affinity
- Monitor network bandwidth
- Verify OpenGL driver updates

#### Spout Not Working
- Windows only feature
- Ensure Spout is installed system-wide
- Check graphics driver compatibility

### Health Monitoring Output

Press 'H' in receiver for detailed health report:
```
Health Status: HEALTHY
Metrics:
  Primary Feed: OK (1.0) - Connected
  Secondary Feed: OK (1.0) - Connected
  Active Feed: OK (0.0) - Primary
  Text Renderer: OK (1.0) - Operational
  Spout Sender: OK (1.0) - Ready
```

### Log Output

Both applications provide detailed console logging:
- Connection establishment/failure
- Message send/receive confirmations
- Automatic failover events
- Performance statistics

## System Requirements

### Minimum
- CPU: Quad-core 2.4GHz
- RAM: 8GB
- GPU: OpenGL 3.3+ support
- Network: Multicast-capable Ethernet

### Recommended
- CPU: 8-core 3.0GHz+ with dedicated cores
- RAM: 16GB+
- GPU: Dedicated graphics with 2GB+ VRAM
- Network: Gigabit Ethernet with multicast optimization

## License

This system uses several open-source libraries:
- Aeron: Apache License 2.0
- GLFW: zlib/libpng License
- FreeType: FreeType License
- ImGui: MIT License
- GLM: MIT License