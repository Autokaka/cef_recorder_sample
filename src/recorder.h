#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include "frame_writer.h"
#include "offscreen_client.h"

namespace pup {

struct RecorderConfig {
  std::string url;
  std::filesystem::path output_dir;
  int width = 1280;
  int height = 720;
  int duration = 5;  // 秒
  int fps = 30;
};

/// 录屏控制器
/// 职责: 协调 CEF 浏览器和帧写入器，管理录制流程
class Recorder {
 public:
  explicit Recorder(RecorderConfig config);
  ~Recorder();

  bool Initialize();
  bool Record();
  void Shutdown();

 private:
  bool WaitForBrowser();
  bool WaitForLoad();

  RecorderConfig config_;
  CefRefPtr<OffscreenClient> client_;
  std::unique_ptr<FrameWriter> writer_;
};

}  // namespace pup
