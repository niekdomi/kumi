#pragma once

namespace kumi::tools::lsp {

// Would be nice to have an lsp.
//
// Lsp will be started with `kumi lsp`. this will spawn a process.
// No additional flags are needed.
class LanguageServer
{
  public:
    auto start() -> void
    {
        // TODO(domi): Not sure if this is a void function or we should return
        // some kind of exit code
    }

  private:
};

} // namespace kumi::tools::lsp
