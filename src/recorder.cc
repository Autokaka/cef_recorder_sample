#include "recorder.h"
#include <include/cef_app.h>
#include <include/wrapper/cef_helpers.h>
#include <iostream>
#include <thread>

namespace pup {

Recorder::Recorder(RecorderConfig config) : config_(std::move(config)) {}

bool Recorder::Initialize() {
  client_ = new OffscreenClient(config_.output_dir, config_.width, config_.height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(nullptr);
  // window_info.external_begin_frame_enabled = true;

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

  client_->SetRecordingEnabled(true);
  std::cout << "Recording...\n";

  auto total_frames = config_.duration * config_.fps;
  auto host = client_->GetBrowser()->GetHost();
  while (client_->GetFrameCount() < total_frames) {
    std::this_thread::sleep_for(1s);
    CefDoMessageLoopWork();
    // host->Invalidate(PET_VIEW);
  }

  client_->SetRecordingEnabled(false);
  std::cout << "Recording... Done!\n";
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
