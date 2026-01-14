#include "devtools_observer.h"
#include <include/cef_app.h>
#include <include/wrapper/cef_helpers.h>

namespace pup {

void DevToolsObserver::EnsureAttached(CefRefPtr<CefBrowserHost> host) {
  CEF_REQUIRE_UI_THREAD();
  if (!registration_ && host) {
    registration_ = host->AddDevToolsMessageObserver(this);
  }
}

bool DevToolsObserver::WaitForResult(int message_id, std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;

  while (std::chrono::steady_clock::now() < deadline) {
    {
      std::lock_guard lock(mutex_);
      if (auto it = results_.find(message_id); it != results_.end()) {
        auto success = it->second;
        results_.erase(it);
        return success;
      }
    }
    CefDoMessageLoopWork();
  }
  return false;
}

void DevToolsObserver::OnDevToolsMethodResult([[maybe_unused]] CefRefPtr<CefBrowser> browser,
                                              int message_id,
                                              bool success,
                                              [[maybe_unused]] const void* result,
                                              [[maybe_unused]] size_t result_size) {
  std::lock_guard lock(mutex_);
  results_[message_id] = success;
}

void DevToolsObserver::OnDevToolsEvent([[maybe_unused]] CefRefPtr<CefBrowser> browser,
                                       const CefString& method,
                                       [[maybe_unused]] const void* params,
                                       [[maybe_unused]] size_t params_size) {
  if (method == "Emulation.virtualTimeBudgetExpired") {
    budget_expired_ = true;
  }
}

void DevToolsObserver::ResetBudgetExpired() {
  budget_expired_ = false;
}

bool DevToolsObserver::WaitForBudgetExpired(std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;

  while (std::chrono::steady_clock::now() < deadline) {
    if (budget_expired_) {
      return true;
    }
    CefDoMessageLoopWork();
  }
  return false;
}

}  // namespace pup
