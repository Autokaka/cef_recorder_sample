#include "recorder.h"
#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/wrapper/cef_helpers.h>
#include <cmath>
#include <iostream>

namespace pup {

Recorder::Recorder(RecorderConfig config) : config_(std::move(config)) {}

bool Recorder::Initialize() {
  client_ = new OffscreenClient(config_.output_dir, config_.width, config_.height);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(nullptr);
  window_info.external_begin_frame_enabled = true;

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = config_.frame_rate;

  CefBrowserHost::CreateBrowser(window_info, client_.get(), config_.url,
                                browser_settings, nullptr, nullptr);

  return WaitForBrowser(std::chrono::seconds(config_.duration_seconds)) &&
         WaitForPageLoad(std::chrono::seconds(10));
}

bool Recorder::Record() {
  if (!ConfigureVirtualTime()) {
    return false;
  }

  client_->SetRecordingEnabled(true);
  CaptureFrames();
  client_->SetRecordingEnabled(false);

  return true;
}

void Recorder::Shutdown() {
  if (auto browser = client_->GetBrowser()) {
    browser->GetHost()->CloseBrowser(true);

    const auto close_start = std::chrono::steady_clock::now();
    while (client_->GetBrowser() &&
           std::chrono::steady_clock::now() - close_start <
               std::chrono::seconds(2)) {
      CefDoMessageLoopWork();
    }
  }

  client_ = nullptr;
}

bool Recorder::WaitForBrowser(std::chrono::seconds timeout) {
  const auto start = std::chrono::steady_clock::now();
  while (!client_->GetBrowser()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - start > timeout) {
      std::cerr << "Timed out waiting for browser creation\n";
      return false;
    }
  }
  return true;
}

bool Recorder::WaitForPageLoad(std::chrono::seconds timeout) {
  const auto start = std::chrono::steady_clock::now();
  while (!client_->IsLoadComplete()) {
    CefDoMessageLoopWork();
    if (std::chrono::steady_clock::now() - start > timeout) {
      std::cerr << "Timed out waiting for page load\n";
      return false;
    }
  }
  return true;
}

bool Recorder::ConfigureVirtualTime() {
  using namespace std::chrono_literals;

  const int page_enable_id = client_->ExecuteDevToolsMethod("Page.enable", nullptr);
  if (!client_->WaitForDevToolsResult(page_enable_id, 2s)) {
    std::cerr << "Failed to enable Page domain\n";
    return false;
  }

  auto virtual_time_params = CefDictionaryValue::Create();
  virtual_time_params->SetString("policy", "pauseIfNetworkFetchesPending");
  virtual_time_params->SetInt("budget", 0);
  virtual_time_params->SetInt("maxVirtualTimeTaskStarvationCount", 500);
  virtual_time_params->SetBool("waitForNavigation", false);
  virtual_time_params->SetDouble("initialVirtualTime", 0.0);

  const int vtime_id = client_->ExecuteDevToolsMethod(
      "Emulation.setVirtualTimePolicy", virtual_time_params);

  if (!client_->WaitForDevToolsResult(vtime_id, 2s)) {
    std::cerr << "Failed to set virtual time policy\n";
    return false;
  }

  return true;
}

void Recorder::CaptureFrames() {
  using namespace std::chrono_literals;

  const int target_frames = config_.frame_rate * config_.duration_seconds;
  const double frame_interval_ms = 1000.0 / config_.frame_rate;
  int accumulated_ms = 0;
  int safety_iters = 0;

  while (client_->GetFrameCount() < target_frames &&
         safety_iters < target_frames * 3) {
    const int frame = client_->GetFrameCount();
    const int target_total_ms =
        static_cast<int>(std::lround((frame + 1) * frame_interval_ms));
    const int frame_budget_ms = std::max(1, target_total_ms - accumulated_ms);
    accumulated_ms += frame_budget_ms;

    if (auto browser = client_->GetBrowser()) {
      if (auto host = browser->GetHost()) {
        auto advance_params = CefDictionaryValue::Create();
        advance_params->SetString("policy", "advance");
        advance_params->SetInt("budget", frame_budget_ms);
        advance_params->SetInt("maxVirtualTimeTaskStarvationCount", 500);

        const int advance_id = client_->ExecuteDevToolsMethod(
            "Emulation.setVirtualTimePolicy", advance_params);
        client_->WaitForDevToolsResult(advance_id, 500ms);

        host->Invalidate(PET_VIEW);
        host->SendExternalBeginFrame();
        client_->WaitForFrameCount(frame + 1, 500ms);
      }
    }

    const int after = client_->GetFrameCount();
    if (after <= frame) {
      ++safety_iters;
    } else {
      safety_iters = 0;
    }

    CefDoMessageLoopWork();
  }

  // Drain any in-flight paint for the last frame(s)
  client_->WaitForFrameCount(target_frames, 2s);
}

}  // namespace pup
