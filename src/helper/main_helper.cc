// Helper Process Entry Point
#include <include/cef_app.h>

#if defined(OS_MAC)
#include <include/wrapper/cef_library_loader.h>
#endif

#include "shared/cef_app.h"

#if defined(OS_MAC)
int main(int argc, char* argv[]) {
  // Load the CEF framework library
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInHelper()) {
    return 1;
  }

  CefMainArgs main_args(argc, argv);

  // Create the helper app (reuse SimpleApp for command line processing)
  CefRefPtr<pup::SimpleApp> app = new pup::SimpleApp();

  // Execute the helper process
  return CefExecuteProcess(main_args, app, nullptr);
}
#endif
