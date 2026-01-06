set_project("kumi")
    set_version("0.1.0")
    set_languages("c++23")
    set_defaultmode("debug")
    set_toolchains("clang")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "." })
add_includedirs("src")

local clang_flags = {
    -- Core
    "-WCL4",
    -- "-Werror",
    "-Wpedantic",
    "-Wdeprecated",

    -- Safety
    "-Wnon-virtual-dtor",
    "-Wsuggest-override",
    "-Wunsafe-buffer-usage",
    "-Wexperimental-lifetime-safety",
    "-Wconsumed",
    "-Wthread-safety",
    "-Wformat=2",

    -- Type Safety & Performance
    "-Wconversion",
    "-Wcast-align",
    "-Wold-style-cast",
    "-Wfloat-equal",
    "-Wdouble-promotion",
    "-Wcast-function-type",
    "-Wzero-as-null-pointer-constant",

    -- Logical & Control Flow
    "-Wimplicit-fallthrough",
    "-Wshadow-all",
    "-Wunreachable-code-aggressive",
    "-Wloop-analysis",
    "-Wcovered-switch-default",

    -- Cross-Platform
    "-Wpoison-system-directories",

    -- Suppress warnings for extensions used by dependencies
    "-Wno-gnu-statement-expression", -- Used in TRY macro
    "-Wno-gnu-case-range", -- Used in lexer switch cases
    "-Wno-c2y-extensions", -- Catch2 uses __COUNTER__
}

target("kumi")
    set_kind("binary")
    add_files("src/main.cpp")
    add_headerfiles("src/**.hpp")

    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
        add_defines("DEBUG")
    else
        set_symbols("hidden")
        add_cxflags("-O3", "-flto", { force = true })
        add_ldflags("-flto", { force = true })
        -- add_cxflags("-O3", "-flto", "-fno-exceptions", { force = true })
        -- add_ldflags("-flto", "-fno-exceptions", { force = true })
        add_defines("NDEBUG")
    end

    add_cxflags(clang_flags, { force = true })
target_end()

add_requires("catch2 3.x", { system = false })

target("kumi_tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/**.cpp")
    add_headerfiles("src/**.hpp")
    add_packages("catch2")

    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
        add_defines("DEBUG")
    else
        set_symbols("hidden")
        add_cxflags("-O3", { force = true })
        -- add_cxflags("-O3", "-fno-exceptions", { force = true })
        add_cxflags("-flto=full", { force = true })
        add_ldflags("-flto=full", { force = true })
        -- add_ldflags("-flto=full", "-fno-exceptions", { force = true })
        add_defines("NDEBUG")
    end

    add_cxflags(clang_flags, { force = true })
    add_tests("all")
target_end()

-- Lint task
task("lint")
    set_menu({
        usage = "xmake lint [options]",
        description = "Run clang-tidy on source files",
        options = {
            { nil, "files", "kv", "all", "Which files to lint (all|source|header)" },
            { nil, "file", "kv", nil, "Specific file(s) to lint (comma-separated or multiple --file flags)" },
            { "j", "jobs", "kv", nil, "Number of parallel jobs (default: nproc)" },
            { "e", "errors", "k", nil, "Treat warnings as errors" }
        }
    })

    on_run(function()
        import("core.base.option")
        import("lib.detect.find_tool")
        import("core.project.config")

        -- Find required tools
        local clang_tidy = find_tool("clang-tidy")
        if not clang_tidy then
            raise("clang-tidy not found! Please install it.")
        end

        -- Use full path for run-clang-tidy since find_tool doesn't work correctly for it
        local run_clang_tidy_path = os.iorun("which run-clang-tidy"):trim()
        if run_clang_tidy_path == "" then
            raise("run-clang-tidy not found! Please install it.")
        end

        -- Ensure compile_commands.json exists
        if not os.isfile("compile_commands.json") then
            print("compile_commands.json not found, building first...")
            os.exec("xmake build")
        end

        -- Get configuration
        local build_type = config.mode() or "debug"
        local files_option = option.get("files") or "all"
        local specific_files = option.get("file")
        local jobs = option.get("jobs") or os.iorun("nproc"):trim()
        local warnings_flag = option.get("errors") and "-warnings-as-errors='*'" or ""

        print("Linting with " .. jobs .. " cores...")

        -- Collect files to lint
        local source_files = {}
        local header_files = {}

        -- If specific files are provided, only lint those
        if specific_files then
            -- Handle comma-separated files or multiple --file flags
            local file_list = {}
            if type(specific_files) == "table" then
                file_list = specific_files
            else
                for file in specific_files:gmatch("[^,]+") do
                    table.insert(file_list, file:trim())
                end
            end

            for _, file in ipairs(file_list) do
                if os.isfile(file) then
                    if file:endswith(".cpp") then
                        table.insert(source_files, file)
                    elseif file:endswith(".hpp") then
                        table.insert(header_files, file)
                    else
                        print("Warning: Unknown file type: " .. file)
                    end
                else
                    print("Warning: File not found: " .. file)
                end
            end
        else
            -- Use the files_option to determine which files to lint
            if files_option == "all" or files_option == "source" then
                local sources = os.iorun("find src tests -name '*.cpp' ! -path '*/build/*'"):trim()
                if sources ~= "" then
                    for _, file in ipairs(sources:split("\n")) do
                        if file ~= "" then
                            table.insert(source_files, file)
                        end
                    end
                end
            end

            if files_option == "all" or files_option == "header" then
                local headers = os.iorun("find src tests -name '*.hpp' ! -path '*/build/*'"):trim()
                if headers ~= "" then
                    for _, file in ipairs(headers:split("\n")) do
                        if file ~= "" then
                            table.insert(header_files, file)
                        end
                    end
                end
            end
        end

        -- Run clang-tidy on source files using run-clang-tidy
        if #source_files > 0 then
            print("Running clang-tidy on " .. #source_files .. " source files...")
            local files_str = table.concat(source_files, " ")
            local cmd = string.format(
                "%s -p . -quiet -j %s --header-filter='.*/(src|tests)/.*\\.hpp$' %s %s",
                run_clang_tidy_path,
                jobs,
                warnings_flag,
                files_str
            )

            local result = os.vexec(cmd)
            if result ~= 0 then
                os.exit(1)
            end
        end

        -- Run clang-tidy on header files using parallel xargs
        if #header_files > 0 then
            print("Running clang-tidy on " .. #header_files .. " header files...")
            local header_list = table.concat(header_files, "\n")
            local xargs_cmd = string.format(
                "echo '%s' | xargs -r -P %s -n 1 %s -p . -quiet --header-filter='.*/(src|tests)/.*\\.hpp$' %s",
                header_list,
                jobs,
                clang_tidy.program,
                warnings_flag
            )

            local result = os.vexec(xargs_cmd)
            if result ~= 0 then
                os.exit(1)
            end
        end

        if #source_files == 0 and #header_files == 0 then
            print("No files to lint.")
        else
            print("✓ Linting complete")
        end
    end)
task_end()
