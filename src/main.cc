#include <include/base/cef_build.h>
#include <include/cef_app.h>
#include <include/wrapper/cef_library_loader.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>
#include "cef_app.h"
#include "recorder.h"

namespace fs = std::filesystem;

namespace {

#if defined(OS_MAC)
std::string GetFrameworkBinaryPath() {
  return std::string(CEF_FRAMEWORK_PATH) + "/Chromium Embedded Framework";
}
#endif

std::optional<fs::path> GetExecutablePath(const char* argv0) {
  std::error_code ec;
  if (auto path = fs::canonical(argv0, ec); !ec) {
    return path;
  }
  return fs::absolute(argv0);
}

bool InitializeCEF(int argc, char* argv[]) {
#if defined(OS_MAC)
  const auto framework_binary = GetFrameworkBinaryPath();
  if (cef_load_library(framework_binary.c_str()) != 1) {
    std::cerr << "Failed to load CEF framework at " << framework_binary << '\n';
    return false;
  }
#endif

  CefMainArgs main_args(argc, argv);
  CefRefPtr<CefApp> app = new pup::SimpleApp();

  // Handle subprocess execution
  if (const int exit_code = CefExecuteProcess(main_args, app, nullptr); exit_code >= 0) {
    std::exit(exit_code);
  }

  const auto resources_dir = std::string(CEF_FRAMEWORK_PATH) + "/Resources";
  const auto exe_path = GetExecutablePath(argv[0]);
  if (!exe_path) {
    std::cerr << "Failed to determine executable path\n";
    return false;
  }

  CefSettings settings;
  settings.no_sandbox = true;
  settings.windowless_rendering_enabled = true;
  settings.external_message_pump = true;
  settings.multi_threaded_message_loop = false;
  settings.log_severity = LOGSEVERITY_DISABLE;
  settings.log_items = LOG_ITEMS_NONE;
  CefString(&settings.framework_dir_path) = CEF_FRAMEWORK_PATH;
  CefString(&settings.browser_subprocess_path) = exe_path->u8string();
  CefString(&settings.resources_dir_path) = resources_dir;
  CefString(&settings.locales_dir_path) = resources_dir + "/locales";

  const auto cache_root = fs::current_path() / "cef_cache_root";
  const auto cache_path = cache_root / "default";
  fs::create_directories(cache_path);
  CefString(&settings.root_cache_path) = cache_root.u8string();
  CefString(&settings.cache_path) = cache_path.u8string();

  if (!CefInitialize(main_args, settings, app, nullptr)) {
    std::cerr << "CefInitialize failed\n";
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (!InitializeCEF(argc, argv)) {
    return 1;
  }

  pup::RecorderConfig config{
      .url = "http://0.0.0.0:8000/",
      .output_dir = fs::current_path() / "out",
      .width = 1280,
      .height = 720,
      .duration_seconds = 10,
      .frame_rate = 30,
  };

  pup::Recorder recorder(config);

  if (!recorder.Initialize()) {
    std::cerr << "Failed to initialize recorder\n";
    CefShutdown();
    return 1;
  }

  if (!recorder.Record()) {
    std::cerr << "Recording failed\n";
    recorder.Shutdown();
    CefShutdown();
    return 1;
  }

  recorder.Shutdown();
  CefShutdown();

  return 0;
}
