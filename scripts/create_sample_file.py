import os


def generate_kumi(mode="normal", size_mb=1.0):
    """
    Generates Kumi build files based on EBNF grammar.
    Modes: 'flat', 'nested', 'normal'
    """
    target_bytes = size_mb * 1024 * 1024
    output = []

    # 1. Header Logic
    if mode == "nested":
        output.append('project nested_stress { version: "1.0.0"; }\n')
    elif mode == "flat":
        output.append('project flat_stress { version: "1.0.0"; }\n')
    else:
        output.append(
            'project kumi_enterprise { version: "2025.12.0"; authors: "Kumi Team"; }\n\n'
        )
        output.append("mixin base_cfg {\n  cpp: 23;\n  warnings: strict;\n}\n\n")
        output.append('dependencies {\n  fmt: "10.2.1";\n  spdlog: "1.12.0";\n}\n\n')

    current_bytes = sum(len(s) for s in output)
    i = 0

    # 2. Body Generation
    while current_bytes < target_bytes:
        block = ""
        if mode == "nested":
            # Deeply nested @if/@else to test Parser Stack/Recursion
            depth = 12
            indent = ""
            for d in range(depth):
                indent = "  " * d
                block += f"{indent}@if platform(linux) {{\n"
            block += f'{indent}  target leaf_{i} {{ type: executable; sources: "main.cpp"; }}\n'
            for d in range(depth - 1, -1, -1):
                indent = "  " * d
                block += f"{indent}}}\n"

        elif mode == "flat":
            # Thousands of small tokens to test Lexer throughput
            block = (
                f"target lib_{i} {{\n"
                f"  type: static-lib;\n"
                f'  sources: "src/file_{i}.cpp";\n'
                f"  optimize: speed;\n"
                f"}}\n"
            )

        else:  # normal
            # Real-world mix of loops, conditionals, and assignments
            choice = i % 3
            if choice == 0:
                block = (
                    f"target service_{i} with base_cfg {{\n"
                    f"  type: executable;\n"
                    f'  sources: "services/{i}/*.cpp";\n'
                    f"  @if arch(x86_64) {{\n"
                    f'    compile-options: "-march=native";\n'
                    f"  }}\n"
                    f"  @for lang in [en, de, fr] {{\n"
                    f'    resources: "locales/${{lang}}/*";\n'
                    f"  }}\n"
                    f"}}\n\n"
                )
            elif choice == 1:
                block = (
                    f"profile release_{i} with base_cfg {{\n"
                    f"  optimize: aggressive;\n"
                    f"  lto: true;\n"
                    f"  strip: true;\n"
                    f"}}\n\n"
                )
            else:
                block = (
                    f"mixin plugin_{i} {{\n"
                    f"  public {{\n"
                    f'    defines: "PLUGIN_ID={i}";\n'
                    f'    include-dirs: "plugins/{i}/include";\n'
                    f"  }}\n"
                    f"}}\n\n"
                )

        output.append(block)
        current_bytes += len(block)
        i += 1

    return "".join(output)


def run_generator(mode, size_mb=1.0):
    filename = f"stress_{mode}.kumi"
    content = generate_kumi(mode, size_mb)
    with open(filename, "w") as f:
        f.write(content)
    actual_size = os.path.getsize(filename) / 1024 / 1024
    print(f"Successfully generated {filename} ({actual_size:.2f} MB)")


if __name__ == "__main__":
    # Generate all modes for testing
    run_generator("normal", 10)
    # run_generator("flat", 2.0)
    # run_generator("nested", 1.0)
