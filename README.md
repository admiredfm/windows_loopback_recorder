# Windows Loopback Recorder

[![pub package](https://img.shields.io/pub/v/windows_loopback_recorder.svg)](https://pub.dev/packages/windows_loopback_recorder)
[![Platform](https://img.shields.io/badge/platform-windows-lightgrey.svg)](https://flutter.dev)

A Flutter plugin for recording mixed audio from Windows system output (speakers) and microphone input simultaneously using WASAPI (Windows Audio Session API).

## 🎯 Features

- **🔊 System Audio Capture**: Record all audio playing through Windows speakers using WASAPI loopback mode
- **🎤 Microphone Recording**: Simultaneously capture microphone input
- **🎵 Real-time Audio Mixing**: Combine system and microphone audio streams in real-time
- **📊 Volume Monitoring**: Real-time volume analysis with RMS, decibel, and percentage values
- **⚙️ High-Quality Resampling**: Built-in libsamplerate integration for professional audio conversion
- **🎛️ Recording Controls**: Start, pause, resume, and stop recording operations
- **📱 Stream Interface**: Get live audio data through Flutter streams
- **🔧 Flexible Configuration**: Customizable sample rate, channels, and bit depth
- **🛡️ Permission Handling**: Automatic Windows microphone permission management
- **📋 Device Management**: List and access available audio capture devices

## 📋 System Requirements

- **OS**: Windows 7 or higher
- **Flutter**: 3.3.0 or higher
- **Dart**: 2.17.0 or higher
- **Build Tools**: Visual Studio with C++ support

## 📦 Installation

Add this to your package's `pubspec.yaml` file:

```yaml
dependencies:
  windows_loopback_recorder: ^0.0.1
```

Then run:

```bash
flutter pub get
```

## 🚀 Quick Start

### Basic Recording

```dart
import 'package:windows_loopback_recorder/windows_loopback_recorder.dart';

final recorder = WindowsLoopbackRecorder();

// Start recording with default settings (44.1kHz, stereo, 16-bit)
await recorder.startRecording();

// Listen to audio stream
recorder.audioStream.listen((audioData) {
  // Process audio data (Uint8List)
  print('Received ${audioData.length} bytes of audio');
});

// Stop recording
await recorder.stopRecording();
```

### Advanced Configuration

```dart
// Start recording with custom audio configuration
await recorder.startRecording(
  config: AudioConfig(
    sampleRate: 16000,    // 16kHz for voice recording
    channels: 1,          // Mono
    bitsPerSample: 16,    // 16-bit depth
  ),
);
```

### Volume Monitoring

```dart
// Start volume monitoring
await recorder.startVolumeMonitoring();

// Listen to volume changes
recorder.volumeStream.listen((volumeData) {
  print('Volume: ${volumeData.percentage}% (${volumeData.decibels} dB)');
});

// Stop volume monitoring
await recorder.stopVolumeMonitoring();
```

## 📚 API Reference

### Core Classes

#### `WindowsLoopbackRecorder`

Main plugin class providing all recording functionality.

#### `AudioConfig`

Audio configuration parameters:

```dart
class AudioConfig {
  final int sampleRate;    // Sample rate: 8000-96000 Hz (default: 44100)
  final int channels;      // Channel count: 1-8 (default: 2)
  final int bitsPerSample; // Bit depth: 16, 24, or 32 (default: 16)
}
```

#### `VolumeData`

Real-time volume information:

```dart
class VolumeData {
  final double rms;        // RMS value (0.0-1.0)
  final double decibels;   // Volume in decibels (-96.0-0.0 dB)
  final int percentage;    // Volume percentage (0-100%)
  final int timestamp;     // Timestamp in milliseconds
}
```

#### `RecordingState`

Current recording status:

```dart
enum RecordingState {
  idle,      // Not recording
  recording, // Currently recording
  paused,    // Recording paused
}
```

### Methods

#### Recording Control

```dart
// Start recording
Future<bool> startRecording({AudioConfig? config})

// Pause current recording
Future<bool> pauseRecording()

// Resume paused recording
Future<bool> resumeRecording()

// Stop recording and cleanup
Future<bool> stopRecording()

// Get current recording state
Future<RecordingState> getRecordingState()
```

#### Permission Management

```dart
// Check microphone permission
Future<bool> hasMicrophonePermission()

// Request microphone permission
Future<bool> requestMicrophonePermission()
```

#### Device Management

```dart
// Get available audio capture devices
Future<List<String>> getAvailableDevices()

// Get current audio format
Future<AudioConfig> getAudioFormat()
```

#### Volume Monitoring

```dart
// Start volume monitoring
Future<bool> startVolumeMonitoring()

// Stop volume monitoring
Future<bool> stopVolumeMonitoring()
```

#### Streams

```dart
// Real-time audio data stream
Stream<Uint8List> get audioStream

// Real-time volume data stream
Stream<VolumeData> get volumeStream
```

## 💡 Complete Example

```dart
import 'package:flutter/material.dart';
import 'package:windows_loopback_recorder/windows_loopback_recorder.dart';
import 'dart:typed_data';

class RecorderWidget extends StatefulWidget {
  @override
  _RecorderWidgetState createState() => _RecorderWidgetState();
}

class _RecorderWidgetState extends State<RecorderWidget> {
  final _recorder = WindowsLoopbackRecorder();
  RecordingState _state = RecordingState.idle;
  VolumeData? _volume;
  List<Uint8List> _audioChunks = [];

  @override
  void initState() {
    super.initState();
    _setupStreams();
  }

  void _setupStreams() {
    // Listen to audio data
    _recorder.audioStream.listen((audioData) {
      setState(() {
        _audioChunks.add(audioData);
      });
    });

    // Listen to volume changes
    _recorder.volumeStream.listen((volumeData) {
      setState(() {
        _volume = volumeData;
      });
    });
  }

  Future<void> _toggleRecording() async {
    if (_state == RecordingState.idle) {
      // Check permission
      if (!await _recorder.hasMicrophonePermission()) {
        await _recorder.requestMicrophonePermission();
      }

      // Start recording and volume monitoring
      await _recorder.startRecording(
        config: AudioConfig(
          sampleRate: 44100,
          channels: 2,
          bitsPerSample: 16,
        ),
      );
      await _recorder.startVolumeMonitoring();
    } else {
      // Stop recording and volume monitoring
      await _recorder.stopRecording();
      await _recorder.stopVolumeMonitoring();
    }

    // Update state
    _state = await _recorder.getRecordingState();
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        // Recording status
        Text('Status: ${_state.toString()}'),
        Text('Audio chunks: ${_audioChunks.length}'),

        // Volume display
        if (_volume != null) ...[
          Text('Volume: ${_volume!.percentage}%'),
          LinearProgressIndicator(
            value: _volume!.percentage / 100.0,
            backgroundColor: Colors.grey[300],
            valueColor: AlwaysStoppedAnimation<Color>(
              _volume!.percentage > 80 ? Colors.red :
              _volume!.percentage > 60 ? Colors.orange : Colors.green,
            ),
          ),
        ],

        // Control button
        ElevatedButton(
          onPressed: _toggleRecording,
          child: Text(_state == RecordingState.idle ? 'Start Recording' : 'Stop Recording'),
        ),
      ],
    );
  }

  @override
  void dispose() {
    _recorder.stopRecording();
    _recorder.stopVolumeMonitoring();
    super.dispose();
  }
}
```

## ⚠️ Important Notes

### Permissions

- **Microphone Access**: Windows will automatically prompt for microphone permission on first use
- **No Additional Manifest**: No special app manifest configuration required for WASAPI

### Audio Quality

- **System Audio**: Captured at native device format, then converted to your specified format
- **Microphone Audio**: Captured at device default format, mixed with system audio
- **Mixing Algorithm**: Each source contributes 50% volume to prevent clipping
- **Resampling**: Uses high-quality libsamplerate for professional audio conversion

### Performance Considerations

- **CPU Usage**: Higher sample rates and channel counts increase CPU usage
- **Memory Usage**: Audio data is streamed in real-time chunks (typically 10ms)
- **Threading**: Audio capture runs on dedicated background thread to prevent UI blocking

### Limitations

- **Windows Only**: This plugin only works on Windows platforms
- **Single Session**: Only one recording session can be active at a time
- **Device Selection**: Currently uses default system devices (future versions may support device selection)

## 🔧 Troubleshooting

### Common Issues

#### Recording Fails to Start

```dart
// Always check permission first
if (!await recorder.hasMicrophonePermission()) {
  bool granted = await recorder.requestMicrophonePermission();
  if (!granted) {
    print('Microphone permission required');
    return;
  }
}
```

#### Poor Audio Quality

- Try different sample rates: 16000 (voice), 44100 (CD quality), 48000 (professional)
- Reduce channels to 1 (mono) for voice recording
- Check system audio settings match your configuration

#### High CPU Usage

- Lower sample rate (e.g., 16000 Hz for voice)
- Use mono instead of stereo
- Ensure adequate CPU resources

#### No Audio Data

```dart
// Check if audio stream is active
recorder.audioStream.listen(
  (data) => print('Audio data: ${data.length} bytes'),
  onError: (error) => print('Stream error: $error'),
  onDone: () => print('Stream ended'),
);
```

### Debug Tips

Enable verbose logging during development:

```dart
// Monitor recording state
Timer.periodic(Duration(seconds: 1), (timer) async {
  final state = await recorder.getRecordingState();
  final format = await recorder.getAudioFormat();
  print('State: $state, Format: ${format.sampleRate}Hz ${format.channels}ch');
});
```

## 🏗️ Technical Implementation

### Architecture

- **WASAPI Integration**: Direct Windows Audio Session API usage for low-latency capture
- **Loopback Recording**: Captures system render endpoint audio without affecting playback
- **Real-time Mixing**: C++ level audio buffer mixing with overflow protection
- **libsamplerate**: Professional-grade sample rate conversion library
- **Flutter EventChannels**: Efficient real-time data streaming to Dart layer

### Audio Pipeline

1. **System Audio**: Captured via WASAPI loopback mode from default render device
2. **Microphone Audio**: Captured via standard WASAPI from default capture device
3. **Format Conversion**: Both streams converted to common format (typically 16-bit PCM)
4. **Mixing**: Real-time combination with 50/50 volume distribution
5. **Resampling**: User-specified format conversion using libsamplerate
6. **Streaming**: Delivered to Flutter via EventChannel in configurable chunk sizes

### Thread Safety

- **Main Thread**: Handles Flutter method calls and UI updates
- **Audio Thread**: Dedicated WASAPI capture and processing thread
- **Event Delivery**: Asynchronous, non-blocking data transmission to Dart

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📞 Support

For issues and feature requests, please use the [GitHub Issues](https://github.com/your-repo/windows_loopback_recorder/issues) page.
