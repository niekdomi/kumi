import os


def generate_cmake(size_mb: float = 10.0) -> str:
    """
    Generates a stress-test CMakeLists.txt in 'normal' style.

    Block types cycle via i % 3:
      0 -> add_executable()   (was: target ... with base_cfg)
      1 -> set_target_properties() release profile  (was: profile ... with base_cfg)
      2 -> add_library(INTERFACE) mixin  (was: mixin ...)
    """
    target_bytes = int(size_mb * 1024 * 1024)
    out: list[str] = []

    # ------------------------------------------------------------------ header
    out.append("# kumi stress test - cmake -P compatible\n\n")

    # base_cfg mixin -> plain variable lists
    out.append("# --- base_cfg mixin\n")
    out.append("set(base_cfg_std cxx_std_23)\n")
    out.append("set(base_cfg_flags -Wall;-Wextra;-Wpedantic)\n\n")

    # dependencies -> just version variables, no network calls
    out.append("# --- dependencies\n")
    out.append('set(dep_fmt_version "10.2.1")\n')
    out.append('set(dep_spdlog_version "1.12.0")\n\n')

    current_bytes = sum(len(s) for s in out)

    # ------------------------------------------------------------------ body
    i = 0
    while current_bytes < target_bytes:
        choice = i % 3

        if choice == 0:
            # target service_i: set() + if() + foreach()
            block = (
                f"# --- target service_{i}\n"
                f"set(service_{i}_type executable)\n"
                f'set(service_{i}_sources "services/{i}/*.cpp")\n'
                f"list(APPEND service_{i}_flags ${{base_cfg_flags}})\n"
                f'if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")\n'
                f"  list(APPEND service_{i}_flags -march=native)\n"
                f"endif()\n"
                f"foreach(lang en de fr)\n"
                f'  list(APPEND service_{i}_resources "locales/${{lang}}/*")\n'
                f"endforeach()\n\n"
            )

        elif choice == 1:
            # profile release_i: set() with optimization flags
            block = (
                f"# --- profile release_{i}\n"
                f"set(release_{i}_optimize aggressive)\n"
                f"set(release_{i}_lto true)\n"
                f"set(release_{i}_strip true)\n"
                f"list(APPEND release_{i}_flags ${{base_cfg_flags}} -O3 -flto -s)\n\n"
            )

        else:
            # mixin plugin_i: set() for defines + include dirs
            block = (
                f"# --- mixin plugin_{i}\n"
                f"set(plugin_{i}_defines PLUGIN_ID={i})\n"
                f'set(plugin_{i}_include_dirs "plugins/{i}/include")\n\n'
            )

        out.append(block)
        current_bytes += len(block)
        i += 1

    return "".join(out)


def run_generator(size_mb: float = 10.0) -> None:
    filename = "stress_normal.cmake"
    content = generate_cmake(size_mb)
    with open(filename, "w") as f:
        f.write(content)
    actual_size = os.path.getsize(filename) / 1024 / 1024
    print(f"Successfully generated {filename} ({actual_size:.2f} MB)")


if __name__ == "__main__":
    run_generator(10.0)
