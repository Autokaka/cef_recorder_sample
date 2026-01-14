#pragma once

#include <include/cef_browser.h>
#include <include/cef_devtools_message_observer.h>
#include <include/cef_registration.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace pup {

class DevToolsObserver final : public CefDevToolsMessageObserver {
 public:
  DevToolsObserver() = default;

  void EnsureAttached(CefRefPtr<CefBrowserHost> host);
  bool WaitForResult(int message_id, std::chrono::milliseconds timeout);
  bool WaitForBudgetExpired(std::chrono::milliseconds timeout);
  void ResetBudgetExpired();

  void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                              int message_id,
                              bool success,
                              const void* result,
                              size_t result_size) override;

  void OnDevToolsEvent(CefRefPtr<CefBrowser> browser,
                       const CefString& method,
                       const void* params,
                       size_t params_size) override;

 private:
  std::mutex mutex_;
  std::unordered_map<int, bool> results_;
  std::atomic<bool> budget_expired_{false};
  CefRefPtr<CefRegistration> registration_;

  IMPLEMENT_REFCOUNTING(DevToolsObserver);
  DISALLOW_COPY_AND_ASSIGN(DevToolsObserver);
};

}  // namespace pup
