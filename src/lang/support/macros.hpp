#pragma once

namespace kumi::lang {

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// @brief Macro for propagating errors in std::expected contexts
///
/// Usage: `[auto result = ]TRY(foo());`
/// If the expected contains an error, it returns early with that error.
#define TRY(expr__)                                                                                \
    ({                                                                                             \
        auto&& result__ = (expr__);                                                                \
        if (!result__.has_value()) [[unlikely]] {                                                  \
            return std::unexpected(std::move(result__).error());                                   \
        }                                                                                          \
        std::move(result__).value();                                                               \
    })

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace kumi::lang
