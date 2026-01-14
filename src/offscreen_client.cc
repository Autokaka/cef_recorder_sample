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
  if (frame_callback_) {
    frame_callback_(buffer, w, h);
  }
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
    loaded_ = true;
  }
}

}  // namespace pup
