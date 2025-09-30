# Windows Loopback Recorder Plugin

## 概述

这是一个Flutter插件，专门用于在Windows平台上录制系统音频（如会议声音、音乐软件声音等）和麦克风音频的混合声音。插件使用Windows WASAPI（Windows Audio Session API）实现低延迟、高质量的音频捕获和混合。

## 功能特性

- ✅ **系统音频录制**: 使用WASAPI loopback模式捕获Windows系统正在播放的所有音频
- ✅ **麦克风音频录制**: 同时捕获麦克风输入音频
- ✅ **实时音频混合**: 将系统音频和麦克风音频实时混合输出
- ✅ **音频流支持**: 提供Stream接口，实时获取混合后的音频数据
- ✅ **录制控制**: 支持开始、暂停、恢复、停止录制
- ✅ **权限管理**: 自动处理Windows麦克风权限
- ✅ **设备管理**: 获取可用音频设备列表
- ✅ **配置灵活**: 支持自定义采样率、声道数、位深度

## API文档

### 主要类

#### `WindowsLoopbackRecorder`

主插件类，提供所有录制功能的接口。

#### `AudioConfig`

音频配置类，用于设置录制参数。

```dart
class AudioConfig {
  final int sampleRate;    // 采样率，默认44100Hz
  final int channels;      // 声道数，默认2（立体声）
  final int bitsPerSample; // 位深度，默认16位
}
```

#### `RecordingState`

录制状态枚举。

```dart
enum RecordingState {
  idle,       // 空闲状态
  recording,  // 录制中
  paused,     // 已暂停
}
```

### 核心API方法

#### 录制控制

##### `Future<bool> startRecording({AudioConfig? config})`

开始录制系统音频和麦克风的混合音频。

**参数:**
- `config` (可选): 音频配置参数

**返回值:**
- `true` - 成功开始录制
- `false` - 开始录制失败

**使用示例:**
```dart
final recorder = WindowsLoopbackRecorder();

// 使用默认配置
bool success = await recorder.startRecording();

// 使用自定义配置
bool success = await recorder.startRecording(
  config: AudioConfig(
    sampleRate: 48000,
    channels: 2,
    bitsPerSample: 16,
  ),
);
```

##### `Future<bool> pauseRecording()`

暂停当前录制。

**返回值:**
- `true` - 成功暂停
- `false` - 暂停失败（如当前不在录制状态）

##### `Future<bool> resumeRecording()`

从暂停状态恢复录制。

**返回值:**
- `true` - 成功恢复录制
- `false` - 恢复失败（如当前不在暂停状态）

##### `Future<bool> stopRecording()`

停止录制并释放所有资源。

**返回值:**
- `true` - 成功停止
- `false` - 停止失败

#### 状态查询

##### `Future<RecordingState> getRecordingState()`

获取当前录制状态。

**返回值:** `RecordingState` 枚举值

#### 权限管理

##### `Future<bool> hasMicrophonePermission()`

检查是否已获得麦克风权限。

##### `Future<bool> requestMicrophonePermission()`

请求麦克风权限（Windows会在首次访问时自动显示权限对话框）。

#### 设备管理

##### `Future<List<String>> getAvailableDevices()`

获取所有可用的音频捕获设备列表。

**返回值:** 设备名称列表

#### 音频流

##### `Stream<Uint8List> get audioStream`

获取实时音频数据流。该流会持续输出混合后的音频数据。

**数据格式:**
- 数据类型: `Uint8List`
- 格式: 16位PCM，根据配置的声道数和采样率
- 混合方式: 系统音频和麦克风音频各占50%音量混合

## 使用示例

### 基本录制示例

```dart
import 'package:flutter/material.dart';
import 'package:windows_loopback_recorder/windows_loopback_recorder.dart';
import 'dart:typed_data';

class AudioRecorderPage extends StatefulWidget {
  @override
  _AudioRecorderPageState createState() => _AudioRecorderPageState();
}

class _AudioRecorderPageState extends State<AudioRecorderPage> {
  final _recorder = WindowsLoopbackRecorder();
  RecordingState _state = RecordingState.idle;
  List<Uint8List> _audioChunks = [];

  @override
  void initState() {
    super.initState();
    _setupAudioStream();
  }

  void _setupAudioStream() {
    _recorder.audioStream.listen((audioData) {
      setState(() {
        _audioChunks.add(audioData);
      });
      // 这里可以将音频数据保存到文件或进行其他处理
    });
  }

  Future<void> _startRecording() async {
    // 检查麦克风权限
    bool hasPermission = await _recorder.hasMicrophonePermission();
    if (!hasPermission) {
      bool granted = await _recorder.requestMicrophonePermission();
      if (!granted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('需要麦克风权限才能录制')),
        );
        return;
      }
    }

    // 开始录制
    bool success = await _recorder.startRecording(
      config: AudioConfig(
        sampleRate: 44100,
        channels: 2,
        bitsPerSample: 16,
      ),
    );

    if (success) {
      _updateState();
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('开始录制失败')),
      );
    }
  }

  Future<void> _pauseRecording() async {
    bool success = await _recorder.pauseRecording();
    if (success) {
      _updateState();
    }
  }

  Future<void> _resumeRecording() async {
    bool success = await _recorder.resumeRecording();
    if (success) {
      _updateState();
    }
  }

  Future<void> _stopRecording() async {
    bool success = await _recorder.stopRecording();
    if (success) {
      _updateState();

      // 处理录制的音频数据
      print('录制完成，共获得 ${_audioChunks.length} 个音频块');

      // 可以将音频数据保存为文件
      // await _saveAudioToFile(_audioChunks);
    }
  }

  Future<void> _updateState() async {
    RecordingState state = await _recorder.getRecordingState();
    setState(() {
      _state = state;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('Windows音频录制')),
      body: Column(
        children: [
          Text('当前状态: ${_state.toString()}'),
          Text('已录制音频块: ${_audioChunks.length}'),

          Row(
            mainAxisAlignment: MainAxisAlignment.spaceEvenly,
            children: [
              ElevatedButton(
                onPressed: _state == RecordingState.idle ? _startRecording : null,
                child: Text('开始录制'),
              ),

              ElevatedButton(
                onPressed: _state == RecordingState.recording ? _pauseRecording : null,
                child: Text('暂停录制'),
              ),

              ElevatedButton(
                onPressed: _state == RecordingState.paused ? _resumeRecording : null,
                child: Text('恢复录制'),
              ),

              ElevatedButton(
                onPressed: _state != RecordingState.idle ? _stopRecording : null,
                child: Text('停止录制'),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
```

### 高级用法 - 保存音频为WAV文件

```dart
import 'dart:io';
import 'dart:typed_data';

class AudioFileWriter {
  static Future<void> saveAsWav(
    List<Uint8List> audioChunks,
    String filePath, {
    int sampleRate = 44100,
    int channels = 2,
    int bitsPerSample = 16,
  }) async {
    // 计算总数据大小
    int totalDataSize = audioChunks.fold(0, (sum, chunk) => sum + chunk.length);

    // WAV文件头
    ByteData header = ByteData(44);

    // RIFF Header
    header.setUint8(0, 0x52);   // 'R'
    header.setUint8(1, 0x49);   // 'I'
    header.setUint8(2, 0x46);   // 'F'
    header.setUint8(3, 0x46);   // 'F'
    header.setUint32(4, totalDataSize + 36, Endian.little); // File size - 8
    header.setUint8(8, 0x57);   // 'W'
    header.setUint8(9, 0x41);   // 'A'
    header.setUint8(10, 0x56);  // 'V'
    header.setUint8(11, 0x45);  // 'E'

    // Format Chunk
    header.setUint8(12, 0x66);  // 'f'
    header.setUint8(13, 0x6D);  // 'm'
    header.setUint8(14, 0x74);  // 't'
    header.setUint8(15, 0x20);  // ' '
    header.setUint32(16, 16, Endian.little);     // Format chunk size
    header.setUint16(20, 1, Endian.little);      // PCM format
    header.setUint16(22, channels, Endian.little);
    header.setUint32(24, sampleRate, Endian.little);
    header.setUint32(28, sampleRate * channels * (bitsPerSample ~/ 8), Endian.little); // Byte rate
    header.setUint16(32, channels * (bitsPerSample ~/ 8), Endian.little); // Block align
    header.setUint16(34, bitsPerSample, Endian.little);

    // Data Chunk
    header.setUint8(36, 0x64);  // 'd'
    header.setUint8(37, 0x61);  // 'a'
    header.setUint8(38, 0x74);  // 't'
    header.setUint8(39, 0x61);  // 'a'
    header.setUint32(40, totalDataSize, Endian.little);

    // 写入文件
    final file = File(filePath);
    final sink = file.openWrite();

    // 写入头部
    sink.add(header.buffer.asUint8List());

    // 写入音频数据
    for (final chunk in audioChunks) {
      sink.add(chunk);
    }

    await sink.close();
  }
}

// 使用示例
await AudioFileWriter.saveAsWav(
  _audioChunks,
  'recording.wav',
  sampleRate: 44100,
  channels: 2,
  bitsPerSample: 16,
);
```

## 技术实现细节

### Windows WASAPI 架构

插件使用以下Windows API实现音频捕获：

1. **系统音频捕获**: 使用`AUDCLNT_STREAMFLAGS_LOOPBACK`标志的WASAPI客户端捕获系统渲染设备的音频输出
2. **麦克风音频捕获**: 使用标准WASAPI捕获客户端从默认音频输入设备获取音频
3. **音频混合**: 在C++层实时混合两路音频流，各占50%音量
4. **数据传输**: 通过Flutter EventChannel将音频数据传输到Dart层

### 线程架构

- **主线程**: 处理Flutter方法调用和UI更新
- **音频捕获线程**: 专门的C++线程处理WASAPI音频捕获和混合
- **事件传输**: 异步传输音频数据到Dart层，不阻塞音频处理

### 音频格式

- **默认格式**: 44.1kHz, 16位PCM, 立体声
- **可配置**: 支持自定义采样率（8000-96000Hz）、声道数（1-8）、位深度（16/24/32位）
- **混合算法**: 简单加法混合，每路音频减半音量防止溢出

## 系统要求

- **操作系统**: Windows 7 或更高版本
- **Flutter SDK**: 3.3.0 或更高版本
- **Dart SDK**: 2.17.0 或更高版本
- **Visual Studio**: 需要C++编译支持

## 权限说明

### 麦克风权限

在Windows 10及更高版本中，应用需要麦克风权限才能访问音频输入设备。插件会在首次尝试访问麦克风时自动触发Windows权限请求对话框。

### 应用清单配置

无需额外的应用清单配置，WASAPI权限由Windows自动管理。

## 故障排除

### 常见问题

1. **无法开始录制**
   - 检查音频设备是否正常连接
   - 确保没有其他应用独占音频设备
   - 检查麦克风权限是否已授予

2. **音频质量问题**
   - 调整AudioConfig参数，尝试不同的采样率和位深度
   - 检查系统音频设置是否与配置匹配

3. **性能问题**
   - 降低采样率或减少声道数
   - 确保应用有足够的CPU资源

### 调试技巧

启用详细日志输出：

```dart
// 在开发模式下监听音频流状态
_recorder.audioStream.listen(
  (data) => print('收到音频数据: ${data.length} 字节'),
  onError: (error) => print('音频流错误: $error'),
  onDone: () => print('音频流结束'),
);
```

## 版本更新

### v0.0.1
- 初始版本
- 基本录制功能
- WASAPI音频捕获
- 实时音频混合
- Flutter EventChannel数据传输

## 许可证

本项目使用MIT许可证。详情请参阅LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request来改进这个插件。

## 联系方式

如有问题或建议，请通过GitHub Issues联系。