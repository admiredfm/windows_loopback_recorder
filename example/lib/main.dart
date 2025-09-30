import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/services.dart';
import 'package:windows_loopback_recorder/windows_loopback_recorder.dart';

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

  @override
  void initState() {
    super.initState();
    _initializePlugin();
  }

  @override
  void dispose() {
    _audioStreamSubscription?.cancel();
    _recorder.stopRecording();
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
        setState(() {
          _audioChunkCount++;
        });
      },
      onError: (error) {
        _setStatusMessage('音频流错误: $error');
      },
    );
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

      // 开始录制
      bool success = await _recorder.startRecording(
        config: AudioConfig(
          sampleRate: 44100,
          channels: 2,
          bitsPerSample: 16,
        ),
      );

      if (success) {
        _setStatusMessage('开始录制成功');
        setState(() {
          _audioChunkCount = 0; // 重置计数器
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
      } else {
        _setStatusMessage('停止录制失败');
      }
      await _updateRecordingState();
    } catch (e) {
      _setStatusMessage('停止录制出错: $e');
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
                    ],
                  ),
                ),
              ),

              SizedBox(height: 16),

              // 设备列表
              Card(
                child: Padding(
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
