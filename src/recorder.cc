#include "recorder.h"
#include <include/cef_app.h>
#include <chrono>
#include <cstring>
#include <iostream>

namespace pup {

Recorder::Recorder(RecorderConfig config) : config_(std::move(config)) {}

Recorder::~Recorder() = default;

bool Recorder::Initialize() {
  auto frame_size = static_cast<size_t>(config_.width) * config_.height * 4;
  writer_ = std::make_unique<FrameWriter>(config_.output_dir, frame_size);
  client_ = new OffscreenClient(config_.width, config_.height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(nullptr);

  CefBrowserSettings settings;
  settings.windowless_frame_rate = config_.fps;

  CefBrowserHost::CreateBrowser(window_info, client_, config_.url, settings, nullptr, nullptr);

  return WaitForBrowser() && WaitForLoad();
}

bool Recorder::WaitForBrowser() {
  auto start = std::chrono::steady_clock::now();
  while (!client_->GetBrowser()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) {
      std::cerr << "Browser creation timeout\n";
      return false;
    }
  }
  return true;
}

bool Recorder::WaitForLoad() {
  auto start = std::chrono::steady_clock::now();
  while (!client_->IsLoaded()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
      std::cerr << "Page load timeout\n";
      return false;
    }
  }
  return true;
}

bool Recorder::Record() {
  auto target_frames = config_.duration * config_.fps;
  auto frame_count = 0;
  auto frame_size = static_cast<size_t>(config_.width) * config_.height * 4;
  auto host = client_->GetBrowser()->GetHost();

  std::cout << "Recording " << target_frames << " frames @ " << config_.fps << " fps...\n";

  client_->SetFrameCallback([&](const void* buffer, int w, int h) {
    if (frame_count >= target_frames) {
      CefQuitMessageLoop();
      return;
    }
    if (w != config_.width || h != config_.height) {
      return;
    }
    writer_->Submit(buffer, frame_count, frame_size);
    frame_count += 1;
  });

  // 录制循环 - 依赖 CEF 的 windowless_frame_rate 自然触发 OnPaint
  // 页面动画时间和录制帧天然对齐
  CefRunMessageLoop();

  client_->SetFrameCallback(nullptr);
  writer_->Flush();
  std::cout << "Wrote " << writer_->GetWrittenCount() << " frames.\n";

  return frame_count == target_frames;
}

void Recorder::Shutdown() {
  if (auto browser = client_->GetBrowser()) {
    browser->GetHost()->CloseBrowser(true);
    auto start = std::chrono::steady_clock::now();
    while (client_->GetBrowser() && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
      CefDoMessageLoopWork();
    }
  }
  writer_.reset();
  client_ = nullptr;
}

}  // namespace pup
