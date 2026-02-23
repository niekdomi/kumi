#pragma once

// NOTE: Not sure if we should use sccache because of more permissive license and i think better cross-platform support.

namespace kumi::build {

/// @brief Wrapper for bundled ccache executable
/// @details Temporary wrapper for the bundled ccache binary
///          Will be replaced by custom CacheBackend implementation
class CcacheWrapper final
{
  public:
};

} // namespace kumi::build
