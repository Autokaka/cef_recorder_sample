#include "offscreen_client.h"
#include <include/cef_app.h>
#include <include/wrapper/cef_helpers.h>

namespace pup {

OffscreenClient::OffscreenClient(int width, int height) : width_(width), height_(height) {}

void OffscreenClient::GetViewRect([[maybe_unused]] CefRefPtr<CefBrowser> browser, CefRect& rect) {
  rect = CefRect(0, 0, width_, height_);
}

void OffscreenClient::OnPaint([[maybe_unused]] CefRefPtr<CefBrowser> browser,
                              PaintElementType type,
                              [[maybe_unused]] const RectList& dirtyRects,
                              const void* buffer,
                              int w,
                              int h) {
  if (type != PET_VIEW || !buffer) {
    return;
  }

  // 只有在录制状态且未达到目标帧数时才处理
  if (recording_enabled_ && frame_id_ < target_frames_) {
    int current_frame = frame_id_.fetch_add(1);

    // 通过回调将帧数据传递给外部处理
    if (frame_callback_) {
      frame_callback_(buffer, w, h, current_frame);
    }

    // 录制完成后自动停止
    if (current_frame + 1 >= target_frames_) {
      recording_enabled_ = false;
    }
  }

  // 标记当前帧已完成
  frame_ready_ = true;
}

void OffscreenClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
}

void OffscreenClient::OnBeforeClose([[maybe_unused]] CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = nullptr;
}

bool OffscreenClient::OnCertificateError([[maybe_unused]] CefRefPtr<CefBrowser> browser,
                                         [[maybe_unused]] cef_errorcode_t cert_error,
                                         [[maybe_unused]] const CefString& request_url,
                                         [[maybe_unused]] CefRefPtr<CefSSLInfo> ssl_info,
                                         CefRefPtr<CefCallback> callback) {
  callback->Continue();
  return true;
}

void OffscreenClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                [[maybe_unused]] int httpStatusCode) {
  if (frame->IsMain() && browser->IsSame(browser_)) {
    load_complete_ = true;
  }
}

void OffscreenClient::StartRecording(int target_frames, FrameCallback callback) {
  frame_id_ = 0;
  target_frames_ = target_frames;
  frame_callback_ = std::move(callback);
  recording_enabled_ = true;
}

void OffscreenClient::StopRecording() {
  recording_enabled_ = false;
  frame_callback_ = nullptr;
}

}  // namespace pup
