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
  int duration = 5;        // 录制时长（秒）
  int fps = 30;            // 帧率
  int writer_threads = 2;  // 异步写入线程数
};

class Recorder {
 public:
  explicit Recorder(RecorderConfig config);
  ~Recorder();

  bool Initialize();
  bool Record();
  void Shutdown();

 private:
  RecorderConfig config_;
  CefRefPtr<OffscreenClient> client_;
  std::unique_ptr<FrameWriter> frame_writer_;
};

}  // namespace pup
