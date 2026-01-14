#pragma once

#include <include/cef_browser.h>
#include <include/cef_devtools_message_observer.h>
#include <include/cef_registration.h>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace pup {

class DevToolsObserver final : public CefDevToolsMessageObserver {
 public:
  DevToolsObserver() = default;

  void EnsureAttached(CefRefPtr<CefBrowserHost> host);
  bool WaitForResult(int message_id, std::chrono::milliseconds timeout);

  void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                              int message_id,
                              bool success,
                              const void* result,
                              size_t result_size) override;

 private:
  std::mutex mutex_;
  std::unordered_map<int, bool> results_;
  CefRefPtr<CefRegistration> registration_;

  IMPLEMENT_REFCOUNTING(DevToolsObserver);
  DISALLOW_COPY_AND_ASSIGN(DevToolsObserver);
};

}  // namespace pup
