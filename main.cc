#include "include/base/cef_build.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_image.h"
#include "include/cef_load_handler.h"
#include "include/cef_render_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_values.h"
#include "include/internal/cef_types.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_library_loader.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static constexpr int kDuration = 10;   // Capture duration in seconds.
static constexpr int kFrameRate = 30;  // Capture frame rate.

class SimpleApp : public CefApp {
 public:
  void OnBeforeCommandLineProcessing(
      const CefString& /*process_type*/,
      CefRefPtr<CefCommandLine> command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");
    command_line->AppendSwitch("allow-insecure-localhost");
    command_line->AppendSwitch("disable-logging");
    command_line->AppendSwitch("disable-gcm-driver");
  }

  IMPLEMENT_REFCOUNTING(SimpleApp);
};

// Simple off-screen (OSR) client that captures every painted frame to disk.
class OffscreenClient : public CefClient,
                        public CefLifeSpanHandler,
                        public CefRenderHandler,
                        public CefRequestHandler,
                        public CefLoadHandler {
 public:
  OffscreenClient(const fs::path& output_dir, int width, int height)
      : output_dir_(output_dir), width_(width), height_(height) {
    fs::create_directories(output_dir_);
  }

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
  bool IsLoadComplete() const { return load_complete_.load(); }
  void SetRecordingEnabled(bool enabled) { recording_enabled_.store(enabled); }
  int GetFrameCount() const { return frame_id_.load(); }

  bool WaitForFrameCount(int target, std::chrono::milliseconds timeout) const {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      if (frame_id_.load() >= target) {
        return true;
      }
      CefDoMessageLoopWork();
    }
    return frame_id_.load() >= target;
  }

  void GetViewRect(CefRefPtr<CefBrowser> /*browser*/, CefRect& rect) override {
    rect = CefRect(0, 0, width_, height_);
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = browser;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = nullptr;
  }

  void OnPaint(CefRefPtr<CefBrowser> /*browser*/,
               PaintElementType type,
               const RectList& /*dirtyRects*/,
               const void* buffer,
               int w,
               int h) override {
    if (type != PET_VIEW || buffer == nullptr) {
      return;
    }
    if (recording_enabled_.load()) {
      SaveFrame(buffer, w, h);
    }
  }

  bool OnCertificateError(CefRefPtr<CefBrowser> /*browser*/,
                          cef_errorcode_t /*cert_error*/,
                          const CefString& /*request_url*/,
                          CefRefPtr<CefSSLInfo> /*ssl_info*/,
                          CefRefPtr<CefCallback> callback) override {
    // Proceed despite certificate errors for this capture demo.
    callback->Continue();
    return true;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int /*httpStatusCode*/) override {
    if (frame->IsMain() && browser->IsSame(browser_)) {
      load_complete_.store(true);
    }
  }

  int ExecuteDevToolsMethod(const std::string& method,
                            CefRefPtr<CefDictionaryValue> params) {
    auto browser = browser_;
    if (!browser)
      return 0;
    auto host = browser->GetHost();
    if (!host)
      return 0;
    devtools_observer_->EnsureAttached(host);
    return host->ExecuteDevToolsMethod(0, method, params);
  }

  bool WaitForDevToolsResult(int message_id,
                             std::chrono::milliseconds timeout) {
    return devtools_observer_->WaitForResult(message_id, timeout);
  }

 private:
  class DevToolsObserver : public CefDevToolsMessageObserver {
   public:
    void EnsureAttached(CefRefPtr<CefBrowserHost> host) {
      CEF_REQUIRE_UI_THREAD();
      if (!registration_ && host) {
        registration_ = host->AddDevToolsMessageObserver(this);
      }
    }

    bool WaitForResult(int message_id, std::chrono::milliseconds timeout) {
      const auto deadline = std::chrono::steady_clock::now() + timeout;
      while (std::chrono::steady_clock::now() < deadline) {
        {
          std::lock_guard<std::mutex> lock(mutex_);
          const auto it = results_.find(message_id);
          if (it != results_.end()) {
            const bool success = it->second;
            results_.erase(it);
            return success;
          }
        }

        CefDoMessageLoopWork();
      }
      return false;
    }

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> /*browser*/,
                                int message_id,
                                bool success,
                                const void* /*result*/,
                                size_t /*result_size*/) override {
      std::lock_guard<std::mutex> lock(mutex_);
      results_[message_id] = success;
    }

   private:
    std::mutex mutex_;
    std::unordered_map<int, bool> results_;
    CefRefPtr<CefRegistration> registration_;

    IMPLEMENT_REFCOUNTING(DevToolsObserver);
  };

  void SaveFrame(const void* buffer, int w, int h) {
    auto image = CefImage::CreateImage();
    const size_t pixel_data_size = static_cast<size_t>(w) * h * 4;
    if (!image->AddBitmap(1.0f, w, h, CEF_COLOR_TYPE_BGRA_8888,
                          CEF_ALPHA_TYPE_PREMULTIPLIED, buffer,
                          pixel_data_size)) {
      return;
    }

    int png_w = 0;
    int png_h = 0;
    auto png = image->GetAsPNG(1.0f, true, png_w, png_h);
    if (!png)
      return;

    std::vector<unsigned char> data(png->GetSize());
    if (!png->GetData(data.data(), data.size(), 0))
      return;

    std::ostringstream filename;
    filename << "frame-" << std::setw(4) << std::setfill('0')
             << frame_id_.fetch_add(1) << ".png";

    const fs::path path = output_dir_ / filename.str();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
  }

  fs::path output_dir_;
  int width_ = 0;
  int height_ = 0;
  std::atomic<int> frame_id_{0};
  std::atomic<bool> load_complete_{false};
  std::atomic<bool> recording_enabled_{false};
  CefRefPtr<CefBrowser> browser_;
  CefRefPtr<DevToolsObserver> devtools_observer_ = new DevToolsObserver();

  IMPLEMENT_REFCOUNTING(OffscreenClient);
};

std::string GetFrameworkBinaryPath() {
#if defined(OS_MAC)
  return std::string(CEF_FRAMEWORK_PATH) + "/Chromium Embedded Framework";
#else
  return std::string();
#endif
}

int main(int argc, char* argv[]) {
#if defined(OS_MAC)
  const std::string framework_binary = GetFrameworkBinaryPath();
  if (framework_binary.empty() ||
      cef_load_library(framework_binary.c_str()) != 1) {
    std::cerr << "Failed to load CEF framework at " << framework_binary
              << std::endl;
    return 1;
  }
#endif

  CefMainArgs main_args(argc, argv);
  CefRefPtr<CefApp> app = new SimpleApp();

  const std::string resources_dir =
      std::string(CEF_FRAMEWORK_PATH) + "/Resources";
  std::error_code ec;
  fs::path exe_path = fs::canonical(argv[0], ec);
  if (ec) {
    exe_path = fs::absolute(argv[0]);
  }

  CefSettings settings;
  settings.no_sandbox = true;
  settings.windowless_rendering_enabled = true;
  settings.external_message_pump = true;
  settings.multi_threaded_message_loop = false;
  settings.log_severity = LOGSEVERITY_DISABLE;
  settings.log_items = LOG_ITEMS_NONE;
  CefString(&settings.framework_dir_path) = CEF_FRAMEWORK_PATH;
  CefString(&settings.browser_subprocess_path) = exe_path.u8string();
  CefString(&settings.resources_dir_path) = resources_dir;
  CefString(&settings.locales_dir_path) = resources_dir + "/locales";

  const fs::path cache_root = fs::current_path() / "cef_cache_root";
  const fs::path cache_path = cache_root / "default";
  fs::create_directories(cache_path);
  CefString(&settings.root_cache_path) = cache_root.u8string();
  CefString(&settings.cache_path) = cache_path.u8string();

  const int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }

  if (!CefInitialize(main_args, settings, app, nullptr)) {
    std::cerr << "CefInitialize failed" << std::endl;
    return 1;
  }

  const fs::path out_dir = fs::current_path() / "out";
  const int width = 1280;
  const int height = 720;

  CefRefPtr<OffscreenClient> client =
      new OffscreenClient(out_dir, width, height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(nullptr);
  window_info.external_begin_frame_enabled = true;

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = 30;

  CefBrowserHost::CreateBrowser(window_info, client.get(),
                                "http://0.0.0.0:8000/", browser_settings,
                                nullptr, nullptr);

  // Wait briefly for the browser to be created.
  const auto wait_start = std::chrono::steady_clock::now();
  while (!client->GetBrowser()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - wait_start >
        std::chrono::seconds(kDuration)) {
      std::cerr << "Timed out waiting for browser creation" << std::endl;
      break;
    }
  }

  // Wait for main frame load completion.
  const auto load_wait_start = std::chrono::steady_clock::now();
  while (!client->IsLoadComplete()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - load_wait_start >
        std::chrono::seconds(10)) {
      std::cerr << "Timed out waiting for page load" << std::endl;
      break;
    }
  }

  auto browser = client->GetBrowser();
  auto host = browser ? browser->GetHost() : nullptr;
  if (!browser || !host) {
    std::cerr << "Browser not available for DevTools configuration"
              << std::endl;
  }

  if (browser && host) {
    const auto two_seconds = std::chrono::milliseconds(2000);
    const int page_enable_id =
        client->ExecuteDevToolsMethod("Page.enable", nullptr);
    client->WaitForDevToolsResult(page_enable_id, two_seconds);

    // Pause virtual time after load; we'll advance per-frame below.
    auto virtual_time_params = CefDictionaryValue::Create();
    virtual_time_params->SetString("policy", "pauseIfNetworkFetchesPending");
    virtual_time_params->SetInt("budget", 0);
    virtual_time_params->SetInt("maxVirtualTimeTaskStarvationCount", 500);
    virtual_time_params->SetBool("waitForNavigation", false);
    virtual_time_params->SetDouble("initialVirtualTime", 0.0);

    const int vtime_id = client->ExecuteDevToolsMethod(
        "Emulation.setVirtualTimePolicy", virtual_time_params);
    client->WaitForDevToolsResult(vtime_id, two_seconds);
  }

  client->SetRecordingEnabled(true);
  const int target_frames = kFrameRate * kDuration;  // 30 fps * 10 seconds
  const double frame_interval_ms = 1000.0 / kFrameRate;
  int accumulated_ms = 0;
  int safety_iters = 0;

  while (client->GetFrameCount() < target_frames &&
         safety_iters < target_frames * 3) {
    const int frame = client->GetFrameCount();
    const int target_total_ms =
        static_cast<int>(std::lround((frame + 1) * frame_interval_ms));
    const int frame_budget_ms = std::max(1, target_total_ms - accumulated_ms);
    accumulated_ms += frame_budget_ms;

    if (auto active_browser = client->GetBrowser()) {
      if (auto active_host = active_browser->GetHost()) {
        auto advance_params = CefDictionaryValue::Create();
        advance_params->SetString("policy", "advance");
        advance_params->SetInt("budget", frame_budget_ms);
        advance_params->SetInt("maxVirtualTimeTaskStarvationCount", 500);

        const int advance_id = client->ExecuteDevToolsMethod(
            "Emulation.setVirtualTimePolicy", advance_params);
        client->WaitForDevToolsResult(advance_id,
                                      std::chrono::milliseconds(500));

        active_host->Invalidate(PET_VIEW);
        active_host->SendExternalBeginFrame();
        client->WaitForFrameCount(frame + 1, std::chrono::milliseconds(500));
      }
    }

    const int after = client->GetFrameCount();
    if (after <= frame) {
      ++safety_iters;  // retry this frame if not advanced
    } else {
      safety_iters = 0;  // reset on progress
    }

    CefDoMessageLoopWork();
  }

  // Drain any in-flight paint for the last frame(s).
  client->WaitForFrameCount(target_frames, std::chrono::seconds(2));
  client->SetRecordingEnabled(false);

  if (auto browser = client->GetBrowser()) {
    browser->GetHost()->CloseBrowser(true);
    const auto close_start = std::chrono::steady_clock::now();
    while (client->GetBrowser() &&
           std::chrono::steady_clock::now() - close_start <
               std::chrono::seconds(2)) {
      CefDoMessageLoopWork();
    }
  }

  CefShutdown();
  return 0;
}
