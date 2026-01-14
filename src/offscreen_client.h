#pragma once

#include <include/cef_browser.h>
#include <include/cef_client.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_load_handler.h>
#include <include/cef_render_handler.h>
#include <include/cef_request_handler.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include "devtools_observer.h"

namespace pup {

class OffscreenClient final : public CefClient,
                              public CefLifeSpanHandler,
                              public CefRenderHandler,
                              public CefRequestHandler,
                              public CefLoadHandler {
 public:
  OffscreenClient(std::filesystem::path output_dir, int width, int height);

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
  void SetRecordingEnabled(bool enabled) { recording_enabled_ = enabled; }
  int GetFrameCount() const { return frame_id_; }

  int ExecuteDevToolsMethod(const std::string& method, CefRefPtr<CefDictionaryValue> params);
  bool WaitForDevToolsResult(int message_id, std::chrono::milliseconds timeout);

 private:
  void SaveFrame(const void* buffer, int width, int height);

  std::filesystem::path output_dir_;
  int width_;
  int height_;
  std::atomic<int> frame_id_{0};
  std::atomic<bool> load_complete_{false};
  std::atomic<bool> recording_enabled_{false};
  CefRefPtr<CefBrowser> browser_;
  CefRefPtr<DevToolsObserver> devtools_observer_{new DevToolsObserver()};

  IMPLEMENT_REFCOUNTING(OffscreenClient);
  DISALLOW_COPY_AND_ASSIGN(OffscreenClient);
};

}  // namespace pup
