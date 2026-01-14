#pragma once

#include <include/cef_browser.h>
#include <include/cef_client.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_load_handler.h>
#include <include/cef_render_handler.h>
#include <include/cef_request_handler.h>
#include <atomic>
#include <functional>
#include <memory>

namespace pup {

// 帧回调函数类型: (buffer, width, height, frame_id)
using FrameCallback = std::function<void(const void*, int, int, int)>;

class OffscreenClient final : public CefClient,
                              public CefLifeSpanHandler,
                              public CefRenderHandler,
                              public CefRequestHandler,
                              public CefLoadHandler {
 public:
  OffscreenClient(int width, int height);

  // CefClient
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  // CefRenderHandler
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override;

  // CefLifeSpanHandler
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefRequestHandler
  bool OnCertificateError(CefRefPtr<CefBrowser> browser,
                          cef_errorcode_t cert_error,
                          const CefString& request_url,
                          CefRefPtr<CefSSLInfo> ssl_info,
                          CefRefPtr<CefCallback> callback) override;

  // CefLoadHandler
  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;

  // Public API
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
  bool IsLoadComplete() const { return load_complete_; }

  // 录制控制
  void StartRecording(int target_frames, FrameCallback callback);
  void StopRecording();
  bool IsRecording() const { return recording_enabled_; }
  int GetFrameCount() const { return frame_id_; }
  int GetTargetFrames() const { return target_frames_; }
  bool IsRecordingComplete() const { return frame_id_ >= target_frames_ && target_frames_ > 0; }

  // 帧同步（用于 external begin frame 模式）
  bool IsFrameReady() const { return frame_ready_; }
  void ResetFrameReady() { frame_ready_ = false; }

 private:
  int width_;
  int height_;
  std::atomic<int> frame_id_{0};
  std::atomic<int> target_frames_{0};
  std::atomic<bool> load_complete_{false};
  std::atomic<bool> recording_enabled_{false};
  std::atomic<bool> frame_ready_{false};  // 当前帧已完成
  FrameCallback frame_callback_;
  CefRefPtr<CefBrowser> browser_;

  IMPLEMENT_REFCOUNTING(OffscreenClient);
  DISALLOW_COPY_AND_ASSIGN(OffscreenClient);
};

}  // namespace pup
