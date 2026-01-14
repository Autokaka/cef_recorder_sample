#pragma once

#include <chrono>
#include <optional>
#include <string>
#include "offscreen_client.h"

namespace pup {

struct RecorderConfig {
  std::string url;
  std::filesystem::path output_dir;
  int width = 1280;
  int height = 720;
  int duration_seconds = 10;
  int frame_rate = 30;
};

class Recorder {
 public:
  explicit Recorder(RecorderConfig config);

  bool Initialize();
  bool Record();
  void Shutdown();

 private:
  bool WaitForBrowser(std::chrono::seconds timeout);
  bool WaitForPageLoad(std::chrono::seconds timeout);
  bool ConfigureVirtualTime();
  void CaptureFrames();

  RecorderConfig config_;
  CefRefPtr<OffscreenClient> client_;
};

}  // namespace pup
