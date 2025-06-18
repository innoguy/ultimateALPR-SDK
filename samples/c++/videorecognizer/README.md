# C++ Video Recognizer

This is a C++ implementation of the Python video recognizer application using the ultimateALPR SDK. It provides real-time license plate recognition and vehicle tracking from video streams.

## Features

- Real-time license plate detection and recognition
- Vehicle tracking across frames using Intersection over Union (IOU)
- Vehicle counting (incoming/outgoing) based on detection zones
- Speed estimation based on vehicle movement
- Video output with annotations
- Support for various image formats and processing options

## Prerequisites

### Required Libraries

1. **OpenCV** (4.x or later)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libopencv-dev
   
   # macOS
   brew install opencv
   
   # Windows
   # Download from https://opencv.org/releases/
   ```

2. **nlohmann/json** (header-only library)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install nlohmann-json3-dev
   
   # macOS
   brew install nlohmann-json
   
   # Or use vcpkg
   vcpkg install nlohmann-json
   ```

3. **cxxopts** (header-only library)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libcxxopts-dev
   
   # macOS
   brew install cxxopts
   
   # Or use vcpkg
   vcpkg install cxxopts
   ```

4. **ultimateALPR SDK**
   - Ensure the SDK is properly installed
   - The library should be available in the `../../../binaries/` directory

### Build Tools

- **CMake** (3.16 or later)
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)

## Building

1. **Create build directory:**
   ```bash
   mkdir build
   cd build
   ```

2. **Configure with CMake:**
   ```bash
   cmake ..
   ```

3. **Build the application:**
   ```bash
   make -j$(nproc)
   ```

## Usage

### Basic Usage

```bash
./videorecognizer --video /path/to/your/video.mp4
```

### Advanced Usage

```bash
./videorecognizer \
    --video /path/to/traffic.mp4 \
    --assets ../../../assets \
    --charset latin \
    --duration 30 \
    --openvino_enabled true \
    --openvino_device CPU \
    --klass_vcr_enabled true \
    --tokenfile /path/to/license.lic
```

### Command Line Options

| Option | Description | Default | Required |
|--------|-------------|---------|----------|
| `--video, -v` | Path to input video file | - | Yes |
| `--assets, -a` | Path to assets folder | `../../../assets` | No |
| `--duration, -d` | Maximum duration to process (seconds) | Entire video | No |
| `--charset, -c` | Recognition charset (latin/korean/chinese) | `latin` | No |
| `--car_noplate_detect_enabled` | Detect cars without plates | `false` | No |
| `--ienv_enabled` | Enable Image Enhancement for Night-Vision | `false` | No |
| `--openvino_enabled` | Enable OpenVINO acceleration | `true` | No |
| `--openvino_device` | OpenVINO device (CPU/GPU/FPGA) | `CPU` | No |
| `--klass_lpci_enabled` | Enable License Plate Country Identification | `false` | No |
| `--klass_vcr_enabled` | Enable Vehicle Color Recognition | `false` | No |
| `--klass_vmmr_enabled` | Enable Vehicle Make Model Recognition | `false` | No |
| `--klass_vbsr_enabled` | Enable Vehicle Body Style Recognition | `false` | No |
| `--tokenfile` | Path to license token file | `""` | No |
| `--tokendata` | Base64 license token data | `""` | No |
| `--help, -h` | Show help message | - | No |

## Output

The application generates:

1. **Annotated Video**: `{input_video_name}_annotated.{extension}`
   - Shows detected license plates with bounding boxes
   - Displays vehicle tracking information
   - Shows incoming/outgoing vehicle counts
   - Displays estimated vehicle speeds

2. **Number Plates File**: `numberplates.txt`
   - Contains all detected license plate numbers

3. **Console Output**:
   - Processing progress
   - Detection results
   - Error messages and warnings

## Detection Zones

The application uses predefined detection zones for vehicle counting:

- **Outgoing Zone**: Left half of the frame, Y-coordinates 55.4% to 60%
- **Incoming Zone**: Right half of the frame, Y-coordinates 36% to 41%

These zones can be modified by changing the `checkBoxout` and `checkBoxin` constants in the source code.

## Vehicle Tracking

The application tracks vehicles across frames using:

- **Text Matching**: Exact license plate text matching
- **IOU Tracking**: Intersection over Union (IOU) threshold of 0.58 for vehicle bounding boxes
- **Speed Estimation**: Based on Y-coordinate changes between frames

## Performance Considerations

- **First Run**: Initial model loading may take several seconds
- **GPU Acceleration**: Enable OpenVINO with GPU for better performance
- **Frame Rate**: Processing speed depends on video resolution and hardware
- **Memory Usage**: Large videos may require significant memory

## Troubleshooting

### Common Issues

1. **Library Not Found**:
   ```bash
   # Set library path
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/ultimateALPR/lib
   ```

2. **OpenCV Not Found**:
   ```bash
   # Install OpenCV development packages
   sudo apt-get install libopencv-dev
   ```

3. **License Issues**:
   - Ensure valid license token is provided
   - Check license file permissions

4. **Video Format Issues**:
   - Ensure video codec is supported by OpenCV
   - Try converting to MP4 format

### Debug Mode

Enable debug output by modifying the JSON configuration in the source code:

```cpp
config["debug_level"] = "debug";
config["debug_write_input_image_enabled"] = true;
```

## License

This code is based on the ultimateALPR SDK samples and is provided for non-commercial use only.

## Support

For issues related to:
- **ultimateALPR SDK**: Visit https://www.doubango.org/SDKs/anpr/
- **This Application**: Check the troubleshooting section above 