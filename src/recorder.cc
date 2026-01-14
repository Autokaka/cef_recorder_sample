#include "recorder.h"
#include <include/cef_app.h>
#include <include/wrapper/cef_helpers.h>
#include <chrono>
#include <cstring>
#include <iostream>

namespace pup {

Recorder::Recorder(RecorderConfig config) : config_(std::move(config)) {}

Recorder::~Recorder() = default;

bool Recorder::Initialize() {
  // 创建异步帧写入器
  frame_writer_ = std::make_unique<FrameWriter>(config_.output_dir, config_.writer_threads);

  // 创建离屏渲染客户端
  client_ = new OffscreenClient(config_.width, config_.height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(nullptr);
  // 注: external_begin_frame_enabled 在某些平台上不可靠，改用 Invalidate 强制重绘

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
  const int total_frames = config_.duration * config_.fps;

  std::cout << "Recording " << total_frames << " frames @ " << config_.fps << " fps...\n";

  // 设置帧回调 - 将裸帧数据提交到异步写入器
  auto frame_callback = [this](const void* buffer, int w, int h, int frame_id) {
    RawFrame frame;
    frame.width = w;
    frame.height = h;
    frame.frame_id = frame_id;

    // 拷贝 BGRA 像素数据
    size_t data_size = static_cast<size_t>(w) * h * 4;
    frame.data.resize(data_size);
    std::memcpy(frame.data.data(), buffer, data_size);

    // 提交到异步写入队列（非阻塞）
    frame_writer_->Submit(std::move(frame));
  };

  // 开始录制
  client_->StartRecording(total_frames, frame_callback);

  auto host = client_->GetBrowser()->GetHost();
  auto record_start = std::chrono::steady_clock::now();
  const auto frame_interval = std::chrono::microseconds(1000000 / config_.fps);
  auto next_frame_time = record_start;

  // 使用 Invalidate 强制重绘来触发 OnPaint
  // 按照目标帧率发送 Invalidate 请求
  while (!client_->IsRecordingComplete()) {
    auto now = std::chrono::steady_clock::now();

    // 按照目标帧率发送 Invalidate 请求
    if (now >= next_frame_time) {
      host->Invalidate(PET_VIEW);
      next_frame_time += frame_interval;
    }

    // 处理 CEF 消息循环
    CefDoMessageLoopWork();

    // 超时保护
    if (now - record_start > std::chrono::seconds(config_.duration + 5)) {
      std::cerr << "Recording timeout, captured " << client_->GetFrameCount() << " frames\n";
      break;
    }
  }

  client_->StopRecording();

  std::cout << "Captured " << client_->GetFrameCount() << " frames, waiting for disk writes...\n";

  // 等待所有帧写入磁盘
  frame_writer_->Flush();

  std::cout << "Recording complete! Wrote " << frame_writer_->GetWrittenCount() << " frames to disk.\n";

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

  // 确保写入器被清理
  frame_writer_.reset();
  client_ = nullptr;
}

}  // namespace pup
