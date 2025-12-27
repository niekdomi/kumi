import os


def generate_kumi(mode="normal", size_mb=1.0):
    """
    Generates Kumi build files based on EBNF grammar.
    Modes: 'flat', 'nested', 'normal'
    """
    target_bytes = size_mb * 1024 * 1024 * 5
    output = []

    # 1. Header Logic
    if mode == "nested":
        output.append('project nested_stress { version: "1.0.0"; }\n')
    elif mode == "flat":
        output.append(':root { --base-ver: "1.0"; }\n')
    else:
        output.append('project kumi_enterprise { version: "2025.12.0"; }\n')
        output.append(":root { --std: 23; --warn: strict; }\n")
        output.append("mixin base_cfg { public { cpp: var(--std); } }\n\n")

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
                block += f"{indent}@if platform(unix) {{\n"
            block += f"{indent}  target leaf_{i} {{ type: executable; }}\n"
            for d in range(depth - 1, -1, -1):
                indent = "  " * d
                block += f"{indent}}}\n"

        elif mode == "flat":
            # Thousands of small tokens to test Lexer throughput & Symbol Table
            block = (
                f"target lib_{i} {{\n"
                f"  type: static-lib;\n"
                f'  sources: "src/file_{i}.cpp";\n'
                f"  defines: ID={i};\n"
                f"  depends: core;\n"
                f"}}\n"
            )

        else:  # normal
            # Real-world mix of loops, conditionals, and assignments
            block = (
                f"target service_{i} extends base_cfg {{\n"
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

        output.append(block)
        current_bytes += len(block)
        i += 1

    return "".join(output)


def run_generator(mode, size=1.0):
    filename = f"stress_{mode}.kumi"
    content = generate_kumi(mode, size)
    with open(filename, "w") as f:
        f.write(content)
    print(
        f"Successfully generated {filename} ({os.path.getsize(filename) / 1024 / 1024:.2f} MB)"
    )


if __name__ == "__main__":
    # Change these params to generate different versions
    # Valid modes: "flat", "nested", "normal"
    TARGET_MODE = "normal"
    TARGET_SIZE = 1.0  # Megabytes

    run_generator(TARGET_MODE, TARGET_SIZE)
