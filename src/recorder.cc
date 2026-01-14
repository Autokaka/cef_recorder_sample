#include "recorder.h"
#include <include/cef_app.h>
#include <chrono>
#include <iostream>

namespace pup {

Recorder::Recorder(RecorderConfig config) : config_(std::move(config)) {}

Recorder::~Recorder() = default;

bool Recorder::Initialize() {
  writer_ = std::make_unique<FrameWriter>(config_.output_dir);
  client_ = new OffscreenClient(config_.width, config_.height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(nullptr);

  CefBrowserSettings settings;
  settings.windowless_frame_rate = config_.fps;

  CefBrowserHost::CreateBrowser(window_info, client_.get(), config_.url, settings, nullptr, nullptr);

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
  target_frames_ = config_.duration * config_.fps;
  frame_count_ = 0;
  const size_t frame_size = static_cast<size_t>(config_.width) * config_.height * 4;

  std::cout << "Recording " << target_frames_ << " frames @ " << config_.fps << " fps...\n";

  // 设置帧回调
  client_->SetFrameCallback([this, frame_size](const void* buffer, int w, int h) {
    if (frame_count_ >= target_frames_)
      return;
    if (w != config_.width || h != config_.height)
      return;

    writer_->Write(frame_count_, buffer, frame_size);
    frame_count_++;
  });

  // 录制循环
  auto host = client_->GetBrowser()->GetHost();
  auto start = std::chrono::steady_clock::now();
  auto frame_interval = std::chrono::microseconds(1000000 / config_.fps);
  auto next_frame = start;

  while (frame_count_ < target_frames_) {
    auto now = std::chrono::steady_clock::now();

    if (now >= next_frame) {
      host->Invalidate(PET_VIEW);
      next_frame += frame_interval;
    }

    CefDoMessageLoopWork();

    if (now - start > std::chrono::seconds(config_.duration + 5)) {
      std::cerr << "Recording timeout at frame " << frame_count_ << "\n";
      break;
    }
  }

  client_->SetFrameCallback(nullptr);

  std::cout << "Captured " << frame_count_ << " frames, flushing...\n";
  writer_->Flush();
  std::cout << "Done! Wrote " << writer_->GetWrittenCount() << " frames.\n";

  return frame_count_ == target_frames_;
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
