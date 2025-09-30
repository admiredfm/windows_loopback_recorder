//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <audioplayers_windows/audioplayers_windows_plugin.h>
#include <windows_loopback_recorder/windows_loopback_recorder_plugin.h>

void RegisterPlugins(flutter::PluginRegistry* registry) {
  AudioplayersWindowsPluginRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("AudioplayersWindowsPlugin"));
  WindowsLoopbackRecorderPluginRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("WindowsLoopbackRecorderPlugin"));
}
