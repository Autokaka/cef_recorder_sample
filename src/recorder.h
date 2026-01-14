#pragma once

#include <string>
#include "offscreen_client.h"

namespace pup {

struct RecorderConfig {
  std::string url;
  std::filesystem::path output_dir;
  int width = 1280;
  int height = 720;
  int duration = 5;  // 5s
  int fps = 30;
};

class Recorder {
 public:
  explicit Recorder(RecorderConfig config);

  bool Initialize();
  bool Record();
  void Shutdown();

 private:
  RecorderConfig config_;
  CefRefPtr<OffscreenClient> client_;
};

}  // namespace pup
