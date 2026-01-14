#include "cef_app.h"

namespace pup {

void SimpleApp::OnBeforeCommandLineProcessing(
    [[maybe_unused]] const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  command_line->AppendSwitch("ignore-certificate-errors");
  command_line->AppendSwitch("allow-insecure-localhost");
  command_line->AppendSwitch("disable-logging");
  command_line->AppendSwitch("disable-gcm-driver");
}

}  // namespace pup
