#include "offscreen_client.h"
#include <include/cef_app.h>
#include <include/cef_image.h>
#include <include/wrapper/cef_helpers.h>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace pup {

OffscreenClient::OffscreenClient(std::filesystem::path output_dir, int width, int height)
    : output_dir_(std::move(output_dir)), width_(width), height_(height) {
  std::filesystem::create_directories(output_dir_);
}

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
  if (recording_enabled_) {
    SaveFrame(buffer, w, h);
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
    load_complete_ = true;
  }
}

int OffscreenClient::ExecuteDevToolsMethod(const std::string& method, CefRefPtr<CefDictionaryValue> params) {
  if (auto host = browser_ ? browser_->GetHost() : nullptr) {
    devtools_observer_->EnsureAttached(host);
    return host->ExecuteDevToolsMethod(0, method, params);
  }
  return 0;
}

bool OffscreenClient::WaitForDevToolsResult(int message_id, std::chrono::milliseconds timeout) {
  return devtools_observer_->WaitForResult(message_id, timeout);
}

void OffscreenClient::SaveFrame(const void* buffer, int w, int h) {
  auto image = CefImage::CreateImage();
  const auto pixel_data_size = static_cast<size_t>(w) * h * 4;

  if (!image->AddBitmap(1.0f, w, h, CEF_COLOR_TYPE_BGRA_8888, CEF_ALPHA_TYPE_PREMULTIPLIED, buffer, pixel_data_size)) {
    return;
  }

  int png_w = 0, png_h = 0;
  auto png = image->GetAsPNG(1.0f, true, png_w, png_h);
  if (!png) {
    return;
  }

  std::vector<unsigned char> data(png->GetSize());
  if (!png->GetData(data.data(), data.size(), 0)) {
    return;
  }

  std::ostringstream filename;
  filename << "frame-" << std::setw(4) << std::setfill('0') << frame_id_.fetch_add(1) << ".png";

  const auto path = output_dir_ / filename.str();
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

}  // namespace pup
