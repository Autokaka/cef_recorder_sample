#include "cef_app.h"

namespace pup {

void SimpleApp::OnBeforeCommandLineProcessing([[maybe_unused]] const CefString& process_type,
                                              CefRefPtr<CefCommandLine> command_line) {
  // 基础设置
  command_line->AppendSwitch("ignore-certificate-errors");
  command_line->AppendSwitch("allow-insecure-localhost");
  command_line->AppendSwitch("disable-logging");
  command_line->AppendSwitch("deterministic-mode");

  // 禁用 GPU 加速，使用软件渲染（更确定性）
  command_line->AppendSwitch("disable-gpu");
  command_line->AppendSwitch("disable-gpu-compositing");
  command_line->AppendSwitch("disable-software-rasterizer");

  // 禁用异步行为
  command_line->AppendSwitch("disable-background-networking");
  command_line->AppendSwitch("disable-background-timer-throttling");
  command_line->AppendSwitch("disable-backgrounding-occluded-windows");
  command_line->AppendSwitch("disable-breakpad");
  command_line->AppendSwitch("disable-component-extensions-with-background-pages");
  command_line->AppendSwitch("disable-features=TranslateUI,BlinkGenPropertyTrees");
  command_line->AppendSwitch("disable-ipc-flooding-protection");
  command_line->AppendSwitch("disable-renderer-backgrounding");

  // 禁用同步相关
  command_line->AppendSwitch("disable-sync");
  command_line->AppendSwitch("disable-gcm-driver");

  // 确保单进程渲染
  command_line->AppendSwitch("disable-extensions");
  command_line->AppendSwitch("disable-plugins");

  // 禁用自动更新和后台任务
  command_line->AppendSwitch("no-first-run");
  command_line->AppendSwitch("no-default-browser-check");
}

}  // namespace pup
