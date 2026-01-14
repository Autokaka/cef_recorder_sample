#pragma once

#include <include/cef_app.h>

namespace pup {

class SimpleApp final : public CefApp {
 public:
  SimpleApp() = default;

  void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;

 private:
  IMPLEMENT_REFCOUNTING(SimpleApp);
  DISALLOW_COPY_AND_ASSIGN(SimpleApp);
};

}  // namespace pup
