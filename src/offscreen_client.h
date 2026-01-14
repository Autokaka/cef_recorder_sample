#pragma once

#include <include/cef_browser.h>
#include <include/cef_client.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_load_handler.h>
#include <include/cef_render_handler.h>
#include <include/cef_request_handler.h>
#include <atomic>
#include <functional>

namespace pup {

/// 帧数据回调: (buffer, width, height)
using OnFrameCallback = std::function<void(const void*, int, int)>;

/// 离屏渲染 CEF 客户端
/// 职责: 管理 CEF 浏览器生命周期，接收渲染帧并通过回调传递
class OffscreenClient final : public CefClient,
                              public CefLifeSpanHandler,
                              public CefRenderHandler,
                              public CefRequestHandler,
                              public CefLoadHandler {
 public:
  OffscreenClient(int width, int height);

  /// 设置帧回调（每次 OnPaint 时调用）
  void SetFrameCallback(OnFrameCallback callback) { frame_callback_ = std::move(callback); }

  /// 浏览器状态
  CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
  bool IsLoaded() const { return loaded_; }

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

 private:
  int width_;
  int height_;
  std::atomic<bool> loaded_{false};
  OnFrameCallback frame_callback_;
  CefRefPtr<CefBrowser> browser_;

  IMPLEMENT_REFCOUNTING(OffscreenClient);
  DISALLOW_COPY_AND_ASSIGN(OffscreenClient);
};

}  // namespace pup
