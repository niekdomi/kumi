#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace kumi {

enum class TokenType : std::uint8_t
{
    // Keywords - Top level
    PROJECT,      // project myapp { }
    WORKSPACE,    // workspace { }
    TARGET,       // target mylib { }
    DEPENDENCIES, // dependencies { }
    DEPS,         // deps { } (alias)
    OPTIONS,      // options { }
    GLOBAL,       // global { }
    MIXIN,        // mixin common { }
    PRESET,       // preset embedded { }
    FEATURES,     // features { }
    TESTING,      // testing { }
    INSTALL,      // install { }
    PACKAGE,      // package { }
    SCRIPTS,      // scripts { }
    TOOLCHAIN,    // toolchain arm { }
    ROOT,         // :root { }

    // Keywords - Visibility
    PUBLIC,    // public { }
    PRIVATE,   // private { }
    INTERFACE, // interface { }

    // Keywords - Properties
    TYPE,     // type: executable;
    SOURCES,  // sources: "*.cpp";
    HEADERS,  // headers: "*.hpp";
    DEPENDS,  // depends: fmt, spdlog;
    APPLY,    // apply: mixin-name;
    INHERITS, // inherits: base-target;
    EXTENDS,  // extends base-target { }
    EXPORT,   // export: true;
    IMPORT,   // import: mylib;

    // Keywords - Control Flow
    IF,             // @if condition { }
    ELSE_IF,        // @else-if condition { }
    ELSE,           // @else { }
    FOR,            // @for item in list { }
    IN,             // @for item in list
    BREAK,          // @break;
    CONTINUE,       // @continue;
    ERROR,          // @error "message";
    WARNING,        // @warning "message";
    IMPORT_KEYWORD, // @import "file.kumi";
    APPLY_KEYWORD,  // @apply profile(x) { }

    // Keywords - Logical
    AND, // condition1 and condition2
    OR,  // condition1 or condition2
    NOT, // not condition

    // Keywords - Functions
    PLATFORM,    // platform(windows)
    ARCH,        // arch(x86_64)
    COMPILER,    // compiler(gcc)
    CONFIG,      // config(debug)
    OPTION,      // option(BUILD_TESTS)
    FEATURE,     // feature(vulkan)
    HAS_FEATURE, // has-feature(avx2)
    EXISTS,      // exists("path/file")
    VAR,         // var(--variable)
    GLOB,        // glob("src/**/*.cpp")
    GIT,         // git("https://...")
    URL,         // url("https://...")
    SYSTEM,      // system { required: true; }

    // Operators & Punctuation
    LEFT_BRACE,    // {
    RIGHT_BRACE,   // }
    LEFT_BRACKET,  // [
    RIGHT_BRACKET, // ]
    LEFT_PAREN,    // (
    RIGHT_PAREN,   // )
    COLON,         // :
    SEMICOLON,     // ;
    COMMA,         // ,
    DOT,           // .
    RANGE,         // ..
    ELLIPSIS,      // ...
    QUESTION,      // ?
    EXCLAMATION,   // !
    ASSIGN,        // =
    EQUAL,         // ==
    NOT_EQUAL,     // !=
    LESS,          // <
    LESS_EQUAL,    // <=
    GREATER,       // >
    GREATER_EQUAL, // >=
    DOLLAR,        // $
    MINUS_MINUS,   // --
    ARROW,         // ->

    // Literals
    IDENTIFIER, // foo, bar_baz
    STRING,     // "hello"
    NUMBER,     // 123
    TRUE,       // true
    FALSE,      // false

    // Special
    END_OF_FILE
};

struct Token final
{
    TokenType type{};
    std::string value;
    std::size_t line{};
    std::size_t column{};
};

} // namespace kumi
