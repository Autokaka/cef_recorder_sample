#include <include/base/cef_build.h>
#include <include/cef_app.h>
#include <include/wrapper/cef_library_loader.h>
#include <cstring>
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

void PrintUsage(const char* program) {
  std::cout << "Usage: " << program << " --url=URL [options]\n"
            << "Options:\n"
            << "  --url=URL           URL to record (required)\n"
            << "  --output=DIR        Output directory (default: ./out)\n"
            << "  --width=N           Video width (default: 1920)\n"
            << "  --height=N          Video height (default: 1080)\n"
            << "  --duration=N        Recording duration in seconds (default: 5)\n"
            << "  --fps=N             Frames per second (default: 30)\n"
            << "  --help              Show this help message\n";
}

std::optional<std::string> GetArgValue(const char* arg, const char* prefix) {
  size_t prefix_len = std::strlen(prefix);
  if (std::strncmp(arg, prefix, prefix_len) == 0) {
    return std::string(arg + prefix_len);
  }
  return std::nullopt;
}

pup::RecorderConfig ParseArgs(int argc, char* argv[]) {
  pup::RecorderConfig config{
      .url = "",
      .output_dir = fs::current_path() / "out",
      .width = 1920,
      .height = 1080,
      .duration = 5,
      .fps = 30,
  };

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];

    if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
      PrintUsage(argv[0]);
      std::exit(0);
    }

    if (auto val = GetArgValue(arg, "--url=")) {
      config.url = *val;
    } else if (auto val = GetArgValue(arg, "--output=")) {
      config.output_dir = *val;
    } else if (auto val = GetArgValue(arg, "--width=")) {
      config.width = std::stoi(*val);
    } else if (auto val = GetArgValue(arg, "--height=")) {
      config.height = std::stoi(*val);
    } else if (auto val = GetArgValue(arg, "--duration=")) {
      config.duration = std::stoi(*val);
    } else if (auto val = GetArgValue(arg, "--fps=")) {
      config.fps = std::stoi(*val);
    }
    // 忽略所有其他参数（CEF 子进程会传入大量内部参数）
  }

  return config;
}

bool InitializeCEF(int argc, char* argv[]) {
#if defined(OS_MAC)
  auto framework_binary = GetFrameworkBinaryPath();
  if (cef_load_library(framework_binary.c_str()) != 1) {
    std::cerr << "Failed to load CEF framework at " << framework_binary << '\n';
    return false;
  }
#endif

  CefMainArgs main_args(argc, argv);
  CefRefPtr<CefApp> app = new pup::SimpleApp();

  // Handle subprocess execution
  if (int exit_code = CefExecuteProcess(main_args, app, nullptr); exit_code >= 0) {
    std::exit(exit_code);
  }

  auto resources_dir = std::string(CEF_FRAMEWORK_PATH) + "/Resources";
  auto exe_path = GetExecutablePath(argv[0]);
  if (!exe_path) {
    std::cerr << "Failed to determine executable path\n";
    return false;
  }

  CefSettings settings;
  settings.windowless_rendering_enabled = true;  // 必须开启
  settings.no_sandbox = true;
  CefString(&settings.framework_dir_path) = CEF_FRAMEWORK_PATH;
  CefString(&settings.browser_subprocess_path) = exe_path->u8string();
  CefString(&settings.resources_dir_path) = resources_dir;
  CefString(&settings.locales_dir_path) = resources_dir + "/locales";

  auto cache_root = fs::current_path() / "cef_cache_root";
  auto cache_path = cache_root / "default";
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
  // 先解析参数（在 CEF 初始化之前，因为子进程也会调用 main）
  auto config = ParseArgs(argc, argv);

  if (!InitializeCEF(argc, argv)) {
    return 1;
  }

  // 检查必传参数（在 CEF 初始化之后，因为子进程会在 InitializeCEF 中 exit）
  if (config.url.empty()) {
    std::cerr << "Error: --url is required\n\n";
    PrintUsage(argv[0]);
    CefShutdown();
    return 1;
  }

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
