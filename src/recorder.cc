#include "recorder.h"
#include <include/cef_app.h>
#include <include/wrapper/cef_helpers.h>
#include <iostream>

namespace pup {

Recorder::Recorder(RecorderConfig config) : config_(std::move(config)) {}

bool Recorder::Initialize() {
  client_ = new OffscreenClient(config_.output_dir, config_.width, config_.height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(nullptr);
  window_info.external_begin_frame_enabled = true;

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = config_.fps;

  CefBrowserHost::CreateBrowser(window_info, client_.get(), config_.url, browser_settings, nullptr, nullptr);

  // 等待浏览器创建
  const auto start = std::chrono::steady_clock::now();
  while (!client_->GetBrowser()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) {
      std::cerr << "Browser creation timeout\n";
      return false;
    }
  }

  // 等待页面加载
  const auto load_start = std::chrono::steady_clock::now();
  while (!client_->IsLoadComplete()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - load_start > std::chrono::seconds(30)) {
      std::cerr << "Page load timeout\n";
      return false;
    }
  }

  return true;
}

bool Recorder::Record() {
  using namespace std::chrono_literals;

  // 启用 Page domain
  const int page_enable_id = client_->ExecuteDevToolsMethod("Page.enable", nullptr);
  if (!client_->WaitForDevToolsResult(page_enable_id, 2s)) {
    std::cerr << "Failed to enable Page domain\n";
    return false;
  }

  // 初始化虚拟时间，policy=pause 确保页面加载后暂停
  auto init_params = CefDictionaryValue::Create();
  init_params->SetString("policy", "pause");
  init_params->SetDouble("initialVirtualTime", 0.0);

  auto init_id = client_->ExecuteDevToolsMethod("Emulation.setVirtualTimePolicy", init_params);
  if (!client_->WaitForDevToolsResult(init_id, 2s)) {
    std::cerr << "Failed to initialize virtual time\n";
    return false;
  }

  client_->SetRecordingEnabled(true);

  auto total_frames = config_.duration * config_.fps;
  auto timestep_ms = 1000.0 / config_.fps;

  for (int i = 0; i < total_frames; ++i) {
    if (i % 30 == 0 || i < 5) {
      std::cout << "Frame " << i << "/" << total_frames << " (virtual time: " << (i * timestep_ms) << "ms)"
                << std::endl;
    }

    // 推进虚拟时间并立即暂停（pause）
    auto advance_params = CefDictionaryValue::Create();
    advance_params->SetString("policy", "advance");
    advance_params->SetDouble("budget", timestep_ms);

    auto advance_id = client_->ExecuteDevToolsMethod("Emulation.setVirtualTimePolicy", advance_params);
    if (!client_->WaitForDevToolsResult(advance_id, 2s)) {
      std::cerr << "Failed to advance virtual time for frame " << i << "\n";
    }

    // 记录当前帧数，准备捕获一帧
    auto current_frame_count = client_->GetFrameCount();

    // 手动触发渲染
    client_->SetRecordingEnabled(true);
    auto host = client_->GetBrowser()->GetHost();
    host->Invalidate(PET_VIEW);
    host->SendExternalBeginFrame();

    // 等待一帧被捕获（只要帧数增加1即可）
    auto target = current_frame_count + 1;
    auto wait_start = std::chrono::steady_clock::now();
    while (client_->GetFrameCount() < target) {
      CefDoMessageLoopWork();
      if (std::chrono::steady_clock::now() - wait_start > std::chrono::seconds(2)) {
        std::cerr << "Frame " << i << " render timeout\n";
        break;
      }
    }

    client_->SetRecordingEnabled(false);
  }

  std::cout << "Captured " << client_->GetFrameCount() << " frames\n";
  return true;
}

void Recorder::Shutdown() {
  if (auto browser = client_->GetBrowser()) {
    browser->GetHost()->CloseBrowser(true);

    const auto start = std::chrono::steady_clock::now();
    while (client_->GetBrowser() && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
      CefDoMessageLoopWork();
    }
  }
  client_ = nullptr;
}

}  // namespace pup
