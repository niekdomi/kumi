-- Kumi Build System
set_project("kumi")
set_version("0.1.0")
set_languages("c++23")

-- Set debug as default mode
set_defaultmode("debug")

-- Use clang toolchain
set_toolchains("clang")

-- Global compiler settings
add_rules("mode.debug", "mode.release")

-- Generate compile_commands.json for clangd
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

-- Add include directory for all targets
add_includedirs("src")

-- Kumi executable
target("kumi")
    set_kind("binary")
    add_files("src/main.cpp")
    add_headerfiles("src/**.hpp")
    
    -- Compiler flags
    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
        add_defines("DEBUG")
    else
        set_symbols("hidden")
        add_cxflags("-O3", {force = true})
        add_defines("NDEBUG")
    end
    
    -- Enable sanitizers in debug mode
    if is_mode("debug") then
        add_cxflags("-fsanitize=address,undefined")
        add_ldflags("-fsanitize=address,undefined")
    end
    
    -- Comprehensive warning flags
    add_cxflags(
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wcast-align",
        "-Wconversion",
        "-Wcovered-switch-default",
        "-Wdeprecated",
        "-Wdouble-promotion",
        "-Wfloat-conversion",
        "-Wfloat-equal",
        "-Wfor-loop-analysis",
        "-Wformat=2",
        "-Winvalid-utf8",
        "-Wmisleading-indentation",
        "-Wnon-virtual-dtor",
        "-Wold-style-cast",
        "-Woverloaded-virtual",
        "-Wpedantic",
        "-Wpoison-system-directories",
        "-Wshadow-all",
        "-Wsign-conversion",
        "-Wsuggest-destructor-override",
        "-Wsuggest-override",
        "-Wsuper-class-method-mismatch",
        "-Wthread-safety",
        "-Wunreachable-code-aggressive",
        "-Wunused",
        "-Wzero-as-null-pointer-constant",

        -- Suppress specific warnings for GNU extensions we intentionally use
        "-Wno-gnu-statement-expression",
        "-Wno-gnu-case-range",
        {force = true}
    )
target_end()

task("lint")
    set_menu {
        usage = "xmake lint",
        description = "Run clang-tidy on all source files",
        options = {
            {'e', "errors", "k", nil, "Treat warnings as errors"}
        }
    }
    
    on_run(function ()
        import("core.base.option")
        import("lib.detect.find_tool")
        
        local clang_tidy = find_tool("clang-tidy")
        if not clang_tidy then
            raise("clang-tidy not found!")
        end
        
        -- Check if compile_commands.json exists
        if not os.isfile("compile_commands.json") then
            print("compile_commands.json not found, building first...")
            os.exec("xmake")
        end
        
        local warnings_flag = option.get("errors") and "-warnings-as-errors='*'" or ""
        
        print("Running clang-tidy...")
        local files = os.iorun("find src -name '*.cpp' -o -name '*.hpp'")
        for _, file in ipairs(files:split('\n')) do
            if file ~= "" then
                print("Checking: " .. file)
                os.execv(clang_tidy.program, {warnings_flag, file})
            end
        end
        print("Done!")
    end)
task_end()
