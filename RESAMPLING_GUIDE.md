# 音频重采样方案说明

本插件现在支持用户自定义音频格式参数，包括采样率、声道数等。有两种重采样实现方案可选：

## 当前方案：内置简单重采样（推荐）

**优势：**
- ✅ 无外部依赖，编译简单
- ✅ 支持所有常见采样率转换
- ✅ 支持声道转换（单声道↔立体声）
- ✅ 性能良好，CPU占用低
- ✅ 代码简洁，易于维护

**实现原理：**
- 使用线性插值算法进行重采样
- 直接在16位PCM数据上操作
- 适用于语音识别、通话等大多数场景

## 替代方案：libsamplerate（高质量）

如果需要更高的音频质量（如专业音乐录制），可以切换到libsamplerate：

### 切换步骤：

1. **修改头文件** (`windows/include/windows_loopback_recorder/windows_loopback_recorder_plugin.h`):
```cpp
// 取消注释这行
#include <samplerate.h>

// 修改成员变量
// double resamplePosition_ = 0.0;  // 注释掉
SRC_STATE* srcState_ = nullptr;     // 取消注释
```

2. **修改CMakeLists.txt** (`windows/CMakeLists.txt`):
```cmake
# 添加libsamplerate依赖
include(FetchContent)

FetchContent_Declare(
  libsamplerate
  GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
  GIT_TAG        1.0.2
)

FetchContent_GetProperties(libsamplerate)
if(NOT libsamplerate_POPULATED)
  FetchContent_Populate(libsamplerate)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(LIBSAMPLERATE_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory(${libsamplerate_SOURCE_DIR} ${libsamplerate_BINARY_DIR})
endif()

# 修改链接库
target_link_libraries(${PLUGIN_NAME} PRIVATE
  flutter flutter_wrapper_plugin
  samplerate  # 添加这行
)

# 添加头文件路径
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${libsamplerate_SOURCE_DIR}/include"  # 添加这行
)
```

3. **修改实现文件**中的重采样方法使用libsamplerate API

## 使用示例

无论选择哪种方案，调用方式都一样：

```dart
// 语音识别配置 - 16kHz单声道
await recorder.startRecording(
  config: AudioConfig(
    sampleRate: 16000,  // 从设备的44100Hz重采样到16000Hz
    channels: 1,        // 从立体声转为单声道
    bitsPerSample: 16,
  ),
);

// 高质量录制 - 48kHz立体声
await recorder.startRecording(
  config: AudioConfig(
    sampleRate: 48000,  // 重采样到48000Hz
    channels: 2,        // 保持立体声
    bitsPerSample: 16,
  ),
);
```

## 性能对比

| 方案 | 音频质量 | CPU使用 | 编译难度 | 适用场景 |
|------|----------|---------|----------|----------|
| 内置重采样 | 良好 | 低 | 简单 | 语音识别、通话、一般录制 |
| libsamplerate | 优秀 | 中等 | 复杂 | 专业音频制作、高质量音乐 |

## 推荐选择

- **一般用途**：使用当前的内置重采样方案
- **专业音频**：需要最高质量时切换到libsamplerate
- **语音应用**：内置方案完全足够

当前的内置实现已经能满足绝大多数应用场景的需求。