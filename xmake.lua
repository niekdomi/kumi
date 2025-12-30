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
	add_cxflags("-O3", { force = true })
	add_cxflags("-march=native", { force = true })
	add_defines("NDEBUG")
end

add_cxflags(clang_flags, { force = true })
target_end()

add_requires("catch2 3.x", { system = false })

target("kumi_tests")
set_kind("binary")
set_default(false)
add_files("tests/*.cpp")
add_headerfiles("src/**.hpp")
add_packages("catch2")

if is_mode("debug") then
	set_symbols("debug")
	set_optimize("none")
	add_defines("DEBUG")
else
	set_symbols("hidden")
	add_cxflags("-O3", { force = true })
	add_defines("NDEBUG")
end

add_cxflags(clang_flags, { force = true })
add_tests("all")
target_end()

-- Lint task
task("lint")
set_menu({
	usage = "xmake lint",
	description = "Run clang-tidy on all source files",
	options = {
		{ "e", "errors", "k", nil, "Treat warnings as errors" },
	},
})

on_run(function()
	import("core.base.option")
	import("lib.detect.find_tool")

	local clang_tidy = find_tool("clang-tidy")
	if not clang_tidy then
		raise("clang-tidy not found!")
	end

	if not os.isfile("compile_commands.json") then
		print("compile_commands.json not found, building first...")
		os.exec("xmake")
	end

	local warnings_flag = option.get("errors") and "-warnings-as-errors='*'" or ""

	print("Running clang-tidy...")
	local files = os.iorun("find src -name '*.cpp' -o -name '*.hpp'")
	for _, file in ipairs(files:split("\n")) do
		if file ~= "" then
			print("Checking: " .. file)
			os.execv(clang_tidy.program, { warnings_flag, file })
		end
	end
	print("Done!")
end)
task_end()
