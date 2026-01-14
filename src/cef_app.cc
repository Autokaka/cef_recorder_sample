#include "cef_app.h"

namespace pup {

void SimpleApp::OnBeforeCommandLineProcessing([[maybe_unused]] const CefString& process_type,
                                              CefRefPtr<CefCommandLine> command_line) {
  // === macOS 专用 ===
  command_line->AppendSwitch("use-mock-keychain");  // 禁用钥匙串弹窗

  // === 禁用不需要的功能 ===
  command_line->AppendSwitch("disable-sync");                            // 禁用同步
  command_line->AppendSwitch("disable-background-networking");           // 禁用后台网络
  command_line->AppendSwitch("disable-component-update");                // 禁用组件更新
  command_line->AppendSwitch("disable-default-apps");                    // 禁用默认应用
  command_line->AppendSwitch("disable-extensions");                      // 禁用扩展
  command_line->AppendSwitch("disable-translate");                       // 禁用翻译
  command_line->AppendSwitch("disable-client-side-phishing-detection");  // 禁用钓鱼检测
  command_line->AppendSwitch("disable-hang-monitor");                    // 禁用卡死检测
  command_line->AppendSwitch("disable-popup-blocking");                  // 禁用弹窗拦截
  command_line->AppendSwitch("disable-prompt-on-repost");                // 禁用重新提交提示
  command_line->AppendSwitch("disable-ipc-flooding-protection");         // 禁用 IPC 洪泛保护

  // === 禁用不需要的服务 ===
  command_line->AppendSwitch("no-first-run");              // 跳过首次运行
  command_line->AppendSwitch("no-default-browser-check");  // 跳过默认浏览器检查
  command_line->AppendSwitch("no-pings");                  // 禁用超链接审计

  // === 性能优化 ===
  command_line->AppendSwitch("disable-breakpad");       // 禁用崩溃报告
  command_line->AppendSwitch("disable-dev-shm-usage");  // 避免 /dev/shm 问题
  command_line->AppendSwitch("disable-features=TranslateUI,BlinkGenPropertyTrees");

  // === 视频播放支持 ===
  command_line->AppendSwitch("enable-media-stream");                                   // 启用媒体流
  command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");  // 允许自动播放

  // === 内存优化 ===
  command_line->AppendSwitch("js-flags=--expose-gc");  // 暴露 GC（可选）
}

}  // namespace pup
