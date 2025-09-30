#include "include/windows_loopback_recorder/windows_loopback_recorder_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "windows_loopback_recorder_plugin.h"

void WindowsLoopbackRecorderPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  windows_loopback_recorder::WindowsLoopbackRecorderPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
