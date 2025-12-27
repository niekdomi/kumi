#pragma once

namespace kumi {

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// @brief Macro for propagating errors in std::expected contexts
///
/// Usage: `auto result = TRY(function_that_returns_expected());`
/// If the expected contains an error, it returns early with that error.
#define TRY(expr__)                                   \
    ({                                                \
        auto &&result__ = (expr__);                   \
        if (!result__) [[unlikely]] {                 \
            return std::unexpected(result__.error()); \
        }                                             \
        std::move(result__).value();                  \
    })

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace kumi
