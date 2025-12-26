#pragma once

namespace kumi {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TRY(expr__)                                   \
    ({                                                \
        auto &&result__ = (expr__);                   \
        if (!result__) [[unlikely]]                   \
            return std::unexpected(result__.error()); \
        std::move(result__).value();                  \
    })

} // namespace kumi
