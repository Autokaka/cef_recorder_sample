#include "cef_app.h"

namespace pup {

void SimpleApp::OnBeforeCommandLineProcessing([[maybe_unused]] const CefString& process_type,
                                              CefRefPtr<CefCommandLine> command_line) {
  // NOTE(luao): Add command line switches if needed.
}

}  // namespace pup
