import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/services.dart';
import 'package:windows_loopback_recorder/windows_loopback_recorder.dart';
import 'package:path_provider/path_provider.dart';
import 'package:path/path.dart' as path;
import 'package:audioplayers/audioplayers.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _platformVersion = 'Unknown';
  final _recorder = WindowsLoopbackRecorder();
  RecordingState _recordingState = RecordingState.idle;
  List<String> _availableDevices = [];
  int _audioChunkCount = 0;
  StreamSubscription<Uint8List>? _audioStreamSubscription;
  String _statusMessage = '';

  // Volume monitoring
  StreamSubscription<VolumeData>? _volumeStreamSubscription;
  bool _volumeMonitoringEnabled = false;
  VolumeData? _currentVolumeData;

  // Audio recording and playback
  final List<Uint8List> _audioChunks = [];
  String? _savedFilePath;
  final AudioPlayer _audioPlayer = AudioPlayer();
  bool _isPlaying = false;
  Duration _playbackPosition = Duration.zero;
  Duration _playbackDuration = Duration.zero;
  AudioConfig? _actualAudioFormat;

  @override
  void initState() {
    super.initState();
    _initializePlugin();
  }

  @override
  void dispose() {
    _audioStreamSubscription?.cancel();
    _volumeStreamSubscription?.cancel();
    _recorder.stopRecording();
    _recorder.stopVolumeMonitoring();
    _audioPlayer.dispose();
    super.dispose();
  }

  Future<void> _initializePlugin() async {
    await _getPlatformVersion();
    await _getAvailableDevices();
    await _updateRecordingState();
    _setupAudioStream();
  }

  Future<void> _getPlatformVersion() async {
    String platformVersion;
    try {
      platformVersion = await _recorder.getPlatformVersion() ?? 'Unknown platform version';
    } on PlatformException {
      platformVersion = 'Failed to get platform version.';
    }

    if (!mounted) return;

    setState(() {
      _platformVersion = platformVersion;
    });
  }

  Future<void> _getAvailableDevices() async {
    try {
      List<String> devices = await _recorder.getAvailableDevices();
      if (!mounted) return;

      setState(() {
        _availableDevices = devices;
      });
    } catch (e) {
      _setStatusMessage('获取设备列表失败: $e');
    }
  }

  void _setupAudioStream() {
    _audioStreamSubscription = _recorder.audioStream.listen(
      (audioData) {
        if (!mounted) return;

        // Collect audio data during recording
        if (_recordingState == RecordingState.recording) {
          _audioChunks.add(Uint8List.fromList(audioData));

          // Debug: Check if audio data contains actual audio (not silence)
          if (_audioChunks.length % 100 == 0) { // Every 100 chunks
            bool hasSoundData = audioData.any((byte) => byte != 0);
            if (hasSoundData) {
              print('音频数据正常 - 块 ${_audioChunks.length}: ${audioData.length} bytes, 包含音频数据');
            } else {
              print('警告: 音频数据块 ${_audioChunks.length} 全为静音');
            }
          }
        }

        setState(() {
          _audioChunkCount++;
        });
      },
      onError: (error) {
        _setStatusMessage('音频流错误: $error');
      },
    );
  }

  void _setupVolumeStream() {
    _volumeStreamSubscription = _recorder.volumeStream.listen(
      (volumeData) {
        if (!mounted) return;

        setState(() {
          _currentVolumeData = volumeData;
        });
      },
      onError: (error) {
        _setStatusMessage('音量流错误: $error');
      },
    );
  }

  Future<void> _startVolumeMonitoring() async {
    try {
      bool success = await _recorder.startVolumeMonitoring();
      if (success) {
        _setupVolumeStream();
        setState(() {
          _volumeMonitoringEnabled = true;
        });
        _setStatusMessage('音量监听已开启');
      } else {
        _setStatusMessage('开启音量监听失败');
      }
    } catch (e) {
      _setStatusMessage('开启音量监听出错: $e');
    }
  }

  Future<void> _stopVolumeMonitoring() async {
    try {
      bool success = await _recorder.stopVolumeMonitoring();
      if (success) {
        await _volumeStreamSubscription?.cancel();
        _volumeStreamSubscription = null;
        setState(() {
          _volumeMonitoringEnabled = false;
          _currentVolumeData = null;
        });
        _setStatusMessage('音量监听已关闭');
      } else {
        _setStatusMessage('关闭音量监听失败');
      }
    } catch (e) {
      _setStatusMessage('关闭音量监听出错: $e');
    }
  }

  Future<void> _updateRecordingState() async {
    try {
      RecordingState state = await _recorder.getRecordingState();
      if (!mounted) return;

      setState(() {
        _recordingState = state;
      });
    } catch (e) {
      _setStatusMessage('获取录制状态失败: $e');
    }
  }

  void _setStatusMessage(String message) {
    if (!mounted) return;
    setState(() {
      _statusMessage = message;
    });

    print("message: $message");

    // 3秒后清除状态消息
    Timer(Duration(seconds: 3), () {
      if (!mounted) return;
      setState(() {
        _statusMessage = '';
      });
    });
  }

  Future<void> _startRecording() async {
    try {
      // 检查权限
      bool hasPermission = await _recorder.hasMicrophonePermission();
      if (!hasPermission) {
        bool granted = await _recorder.requestMicrophonePermission();
        if (!granted) {
          _setStatusMessage('需要麦克风权限才能录制');
          return;
        }
      }

      // 开始录制 - 使用高质量音频配置以获得最佳音质
      bool success = await _recorder.startRecording(
        config: AudioConfig(
          sampleRate: 16000,  // 使用标准CD质量采样率
          channels: 1,        // 立体声以保留音频空间信息
          bitsPerSample: 16,  // 16位深度平衡质量和文件大小
        ),
      );

      if (success) {
        _setStatusMessage('开始录制成功');
        setState(() {
          _audioChunkCount = 0; // 重置计数器
          _audioChunks.clear(); // 清空之前的录音数据
          _savedFilePath = null; // 清空保存的文件路径
        });

        // 自动启动音量监听
        if (!_volumeMonitoringEnabled) {
          await _startVolumeMonitoring();
        }

        // 延迟获取音频格式，避免时序问题
        Timer(Duration(milliseconds: 500), () async {
          try {
            _actualAudioFormat = await _recorder.getAudioFormat();
            if (mounted) {
              print('=== 音频格式调试信息 ===');
              print('用户配置: 44100Hz, 2ch, 16bit');
              print('实际格式: ${_actualAudioFormat!.sampleRate}Hz, ${_actualAudioFormat!.channels}ch, ${_actualAudioFormat!.bitsPerSample}bit');
              print('重采样状态: ${44100 != _actualAudioFormat!.sampleRate || 2 != _actualAudioFormat!.channels ? "需要" : "不需要"}');

              _setStatusMessage('录制中 - 实际格式: ${_actualAudioFormat!.sampleRate}Hz, ${_actualAudioFormat!.channels}ch, ${_actualAudioFormat!.bitsPerSample}bit');
            }
          } catch (e) {
            // 如果获取格式失败，使用默认配置
            _actualAudioFormat = AudioConfig(sampleRate: 44100, channels: 2, bitsPerSample: 16);
            if (mounted) {
              _setStatusMessage('获取音频格式失败，使用默认格式: $e');
            }
          }
        });
      } else {
        _setStatusMessage('开始录制失败');
      }

      await _updateRecordingState();
    } catch (e) {
      _setStatusMessage('开始录制出错: $e');
    }
  }

  Future<void> _pauseRecording() async {
    try {
      bool success = await _recorder.pauseRecording();
      if (success) {
        _setStatusMessage('暂停录制成功');
      } else {
        _setStatusMessage('暂停录制失败');
      }
      await _updateRecordingState();
    } catch (e) {
      _setStatusMessage('暂停录制出错: $e');
    }
  }

  Future<void> _resumeRecording() async {
    try {
      bool success = await _recorder.resumeRecording();
      if (success) {
        _setStatusMessage('恢复录制成功');
      } else {
        _setStatusMessage('恢复录制失败');
      }
      await _updateRecordingState();
    } catch (e) {
      _setStatusMessage('恢复录制出错: $e');
    }
  }

  Future<void> _stopRecording() async {
    try {
      bool success = await _recorder.stopRecording();
      if (success) {
        _setStatusMessage('停止录制成功，共录制 $_audioChunkCount 个音频块');

        // 自动保存录音文件
        if (_audioChunks.isNotEmpty) {
          await _saveRecording();
        } else {
          _setStatusMessage('停止录制成功，但没有录制到音频数据');
        }
      } else {
        _setStatusMessage('停止录制失败');
      }
      await _updateRecordingState();
    } catch (e) {
      _setStatusMessage('停止录制出错: $e');
    }
  }

  // 保存录音为WAV文件
  Future<void> _saveRecording() async {
    try {
      if (_audioChunks.isEmpty) {
        _setStatusMessage('没有录音数据可保存');
        return;
      }

      // 获取Documents目录
      final directory = await getApplicationDocumentsDirectory();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      final fileName = 'recording_$timestamp.wav';
      final filePath = path.join(directory.path, fileName);

      // 创建WAV文件
      await _createWavFile(filePath);

      setState(() {
        _savedFilePath = filePath;
      });

      _setStatusMessage('录音已保存到: $fileName');
    } catch (e) {
      _setStatusMessage('保存录音失败: $e');
    }
  }

  // 创建WAV文件
  Future<void> _createWavFile(String filePath) async {
    // 计算总音频数据大小
    int totalDataSize = _audioChunks.fold(0, (sum, chunk) => sum + chunk.length);

    // 使用实际的音频格式参数
    final format = _actualAudioFormat ?? AudioConfig();
    final int sampleRate = format.sampleRate;
    final int channels = format.channels;
    final int bitsPerSample = format.bitsPerSample;
    final int byteRate = sampleRate * channels * (bitsPerSample ~/ 8);
    final int blockAlign = channels * (bitsPerSample ~/ 8);

    print('=== WAV文件保存调试信息 ===');
    print('目标格式: ${sampleRate}Hz, ${channels}ch, ${bitsPerSample}bit');
    print('音频数据大小: $totalDataSize bytes, 音频块数: ${_audioChunks.length}');
    print('预期播放时长: ${(totalDataSize / (sampleRate * channels * (bitsPerSample / 8))).toStringAsFixed(2)} 秒');

    // 检查音频数据完整性
    int nonZeroChunks = 0;
    for (final chunk in _audioChunks) {
      if (chunk.any((byte) => byte != 0)) {
        nonZeroChunks++;
      }
    }
    print('包含音频数据的块数: $nonZeroChunks / ${_audioChunks.length}');

    if (nonZeroChunks == 0) {
      print('警告: 所有音频数据都是静音！');
    }

    final file = File(filePath);
    final sink = file.openWrite();

    // 写入WAV头部
    final header = ByteData(44);

    // RIFF Header
    header.setUint8(0, 0x52); // 'R'
    header.setUint8(1, 0x49); // 'I'
    header.setUint8(2, 0x46); // 'F'
    header.setUint8(3, 0x46); // 'F'
    header.setUint32(4, totalDataSize + 36, Endian.little); // File size - 8

    // WAVE Header
    header.setUint8(8, 0x57);   // 'W'
    header.setUint8(9, 0x41);   // 'A'
    header.setUint8(10, 0x56);  // 'V'
    header.setUint8(11, 0x45);  // 'E'

    // Format Chunk
    header.setUint8(12, 0x66);  // 'f'
    header.setUint8(13, 0x6D);  // 'm'
    header.setUint8(14, 0x74);  // 't'
    header.setUint8(15, 0x20);  // ' '
    header.setUint32(16, 16, Endian.little);      // Format chunk size
    header.setUint16(20, 1, Endian.little);       // PCM format
    header.setUint16(22, channels, Endian.little);
    header.setUint32(24, sampleRate, Endian.little);
    header.setUint32(28, byteRate, Endian.little);
    header.setUint16(32, blockAlign, Endian.little);
    header.setUint16(34, bitsPerSample, Endian.little);

    // Data Chunk
    header.setUint8(36, 0x64);  // 'd'
    header.setUint8(37, 0x61);  // 'a'
    header.setUint8(38, 0x74);  // 't'
    header.setUint8(39, 0x61);  // 'a'
    header.setUint32(40, totalDataSize, Endian.little);

    // 写入头部
    sink.add(header.buffer.asUint8List());

    // 写入音频数据
    for (final chunk in _audioChunks) {
      sink.add(chunk);
    }

    await sink.close();
  }

  // 播放录音
  Future<void> _playRecording() async {
    try {
      if (_savedFilePath == null || !File(_savedFilePath!).existsSync()) {
        _setStatusMessage('没有找到录音文件');
        return;
      }

      await _audioPlayer.play(DeviceFileSource(_savedFilePath!));
      setState(() {
        _isPlaying = true;
      });
      _setStatusMessage('开始播放录音');

      // 监听播放状态
      _audioPlayer.onPlayerStateChanged.listen((PlayerState state) {
        if (mounted) {
          setState(() {
            _isPlaying = state == PlayerState.playing;
          });
        }
      });

      // 监听播放位置
      _audioPlayer.onPositionChanged.listen((Duration position) {
        if (mounted) {
          setState(() {
            _playbackPosition = position;
          });
        }
      });

      // 监听音频时长
      _audioPlayer.onDurationChanged.listen((Duration duration) {
        if (mounted) {
          setState(() {
            _playbackDuration = duration;
          });
        }
      });

      // 监听播放完成
      _audioPlayer.onPlayerComplete.listen((_) {
        if (mounted) {
          setState(() {
            _isPlaying = false;
            _playbackPosition = Duration.zero;
          });
          _setStatusMessage('播放完成');
        }
      });
    } catch (e) {
      _setStatusMessage('播放录音失败: $e');
    }
  }

  // 停止播放
  Future<void> _stopPlayback() async {
    try {
      await _audioPlayer.stop();
      setState(() {
        _isPlaying = false;
        _playbackPosition = Duration.zero;
      });
      _setStatusMessage('停止播放');
    } catch (e) {
      _setStatusMessage('停止播放失败: $e');
    }
  }

  String _getStateDisplayText() {
    switch (_recordingState) {
      case RecordingState.idle:
        return '空闲';
      case RecordingState.recording:
        return '录制中';
      case RecordingState.paused:
        return '已暂停';
    }
  }

  Color _getVolumeColor(int percentage) {
    if (percentage >= 80) {
      return Colors.red;
    } else if (percentage >= 60) {
      return Colors.orange;
    } else if (percentage >= 30) {
      return Colors.green;
    } else {
      return Colors.blue;
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Windows音频录制插件示例'),
          backgroundColor: Colors.blue,
          foregroundColor: Colors.white,
        ),
        body: Padding(
          padding: EdgeInsets.all(16.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // 平台信息
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('平台信息', style: Theme.of(context).textTheme.titleMedium),
                      SizedBox(height: 8),
                      Text('运行环境: $_platformVersion'),
                    ],
                  ),
                ),
              ),

              SizedBox(height: 16),

              // 录制状态
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('录制状态', style: Theme.of(context).textTheme.titleMedium),
                      SizedBox(height: 8),
                      Text('当前状态: ${_getStateDisplayText()}'),
                      Text('音频块数量: $_audioChunkCount'),
                      if (_actualAudioFormat != null) ...[
                        SizedBox(height: 4),
                        Text('音频格式: ${_actualAudioFormat!.sampleRate}Hz, ${_actualAudioFormat!.channels}ch, ${_actualAudioFormat!.bitsPerSample}bit',
                             style: TextStyle(color: Colors.blue.shade600, fontSize: 12)),
                      ],
                      if (_savedFilePath != null) ...[
                        SizedBox(height: 4),
                        Text('已保存文件: ${path.basename(_savedFilePath!)}',
                             style: TextStyle(color: Colors.green.shade700, fontWeight: FontWeight.w500)),
                      ],
                      if (_isPlaying) ...[
                        SizedBox(height: 4),
                        Text('播放进度: ${_playbackPosition.inSeconds}s / ${_playbackDuration.inSeconds}s',
                             style: TextStyle(color: Colors.blue.shade700)),
                      ],
                      if (_statusMessage.isNotEmpty) ...[
                        SizedBox(height: 8),
                        Container(
                          padding: EdgeInsets.all(8),
                          decoration: BoxDecoration(
                            color: Colors.blue.shade50,
                            borderRadius: BorderRadius.circular(4),
                            border: Border.all(color: Colors.blue.shade200),
                          ),
                          child: Text(_statusMessage, style: TextStyle(color: Colors.blue.shade800)),
                        ),
                      ],
                    ],
                  ),
                ),
              ),

              SizedBox(height: 16),

              // Volume Monitoring
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Text('音量监听', style: Theme.of(context).textTheme.titleMedium),
                          ElevatedButton.icon(
                            onPressed: _volumeMonitoringEnabled ? _stopVolumeMonitoring : _startVolumeMonitoring,
                            icon: Icon(_volumeMonitoringEnabled ? Icons.volume_off : Icons.volume_up),
                            label: Text(_volumeMonitoringEnabled ? '关闭监听' : '开启监听'),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: _volumeMonitoringEnabled ? Colors.red : Colors.green,
                            ),
                          ),
                        ],
                      ),

                      SizedBox(height: 16),

                      if (_volumeMonitoringEnabled && _currentVolumeData != null) ...[
                        // Volume percentage bar
                        Row(
                          children: [
                            Text('音量: ', style: TextStyle(fontWeight: FontWeight.w500)),
                            Expanded(
                              child: LinearProgressIndicator(
                                value: _currentVolumeData!.percentage / 100.0,
                                backgroundColor: Colors.grey[300],
                                valueColor: AlwaysStoppedAnimation<Color>(
                                  _getVolumeColor(_currentVolumeData!.percentage),
                                ),
                                minHeight: 20,
                              ),
                            ),
                            SizedBox(width: 8),
                            Text(
                              '${_currentVolumeData!.percentage}%',
                              style: TextStyle(
                                fontWeight: FontWeight.bold,
                                color: _getVolumeColor(_currentVolumeData!.percentage),
                              ),
                            ),
                          ],
                        ),

                        SizedBox(height: 8),

                        // Volume details
                        Row(
                          children: [
                            Expanded(
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    '分贝值: ${_currentVolumeData!.decibels.toStringAsFixed(1)} dB',
                                    style: TextStyle(fontSize: 14, color: Colors.grey[600]),
                                  ),
                                  Text(
                                    'RMS: ${_currentVolumeData!.rms.toStringAsFixed(3)}',
                                    style: TextStyle(fontSize: 14, color: Colors.grey[600]),
                                  ),
                                ],
                              ),
                            ),
                            // Volume level indicator
                            Container(
                              width: 60,
                              height: 40,
                              decoration: BoxDecoration(
                                border: Border.all(color: Colors.grey),
                                borderRadius: BorderRadius.circular(4),
                              ),
                              child: Column(
                                mainAxisAlignment: MainAxisAlignment.end,
                                children: List.generate(10, (index) {
                                  final level = (9 - index) * 10; // 90, 80, 70, ... 0
                                  final isActive = _currentVolumeData!.percentage > level;
                                  final color = level >= 80 ? Colors.red :
                                              level >= 60 ? Colors.orange :
                                              Colors.green;
                                  return Container(
                                    height: 3,
                                    margin: EdgeInsets.symmetric(horizontal: 4, vertical: 0.5),
                                    decoration: BoxDecoration(
                                      color: isActive ? color : Colors.grey[300],
                                      borderRadius: BorderRadius.circular(1),
                                    ),
                                  );
                                }),
                              ),
                            ),
                          ],
                        ),
                      ] else if (_volumeMonitoringEnabled) ...[
                        Center(
                          child: Padding(
                            padding: EdgeInsets.symmetric(vertical: 20),
                            child: Text(
                              '等待音量数据...',
                              style: TextStyle(color: Colors.grey[600]),
                            ),
                          ),
                        ),
                      ] else ...[
                        Center(
                          child: Padding(
                            padding: EdgeInsets.symmetric(vertical: 20),
                            child: Text(
                              '音量监听未启用',
                              style: TextStyle(color: Colors.grey[600]),
                            ),
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
              ),

              SizedBox(height: 16),

              // 控制按钮
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('录制控制', style: Theme.of(context).textTheme.titleMedium),
                      SizedBox(height: 16),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: [
                          ElevatedButton.icon(
                            onPressed: _recordingState == RecordingState.idle ? _startRecording : null,
                            icon: Icon(Icons.play_arrow),
                            label: Text('开始录制'),
                            style: ElevatedButton.styleFrom(backgroundColor: Colors.green),
                          ),
                          ElevatedButton.icon(
                            onPressed: _recordingState == RecordingState.recording ? _pauseRecording : null,
                            icon: Icon(Icons.pause),
                            label: Text('暂停录制'),
                            style: ElevatedButton.styleFrom(backgroundColor: Colors.orange),
                          ),
                          ElevatedButton.icon(
                            onPressed: _recordingState == RecordingState.paused ? _resumeRecording : null,
                            icon: Icon(Icons.play_arrow),
                            label: Text('恢复录制'),
                            style: ElevatedButton.styleFrom(backgroundColor: Colors.blue),
                          ),
                          ElevatedButton.icon(
                            onPressed: _recordingState != RecordingState.idle ? _stopRecording : null,
                            icon: Icon(Icons.stop),
                            label: Text('停止录制'),
                            style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
                          ),
                        ],
                      ),

                      SizedBox(height: 16),

                      // 播放控制
                      Text('播放控制', style: Theme.of(context).textTheme.titleMedium),
                      SizedBox(height: 16),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: [
                          ElevatedButton.icon(
                            onPressed: (_savedFilePath != null && !_isPlaying && _recordingState == RecordingState.idle)
                                ? _playRecording : null,
                            icon: Icon(Icons.play_arrow),
                            label: Text('播放录音'),
                            style: ElevatedButton.styleFrom(backgroundColor: Colors.purple),
                          ),
                          ElevatedButton.icon(
                            onPressed: _isPlaying ? _stopPlayback : null,
                            icon: Icon(Icons.stop),
                            label: Text('停止播放'),
                            style: ElevatedButton.styleFrom(backgroundColor: Colors.grey),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              ),

              SizedBox(height: 16),

              // 设备列表
              Card(
                child: SingleChildScrollView(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Text('可用音频设备', style: Theme.of(context).textTheme.titleMedium),
                          TextButton.icon(
                            onPressed: _getAvailableDevices,
                            icon: Icon(Icons.refresh),
                            label: Text('刷新'),
                          ),
                        ],
                      ),
                      SizedBox(height: 8),
                      if (_availableDevices.isEmpty)
                        Text('暂无可用设备')
                      else
                        ...(_availableDevices.map((device) => Padding(
                          padding: EdgeInsets.symmetric(vertical: 2),
                          child: Row(
                            children: [
                              Icon(Icons.mic, size: 16, color: Colors.grey),
                              SizedBox(width: 8),
                              Expanded(child: Text(device, style: TextStyle(fontSize: 14))),
                            ],
                          ),
                        )).toList()),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
