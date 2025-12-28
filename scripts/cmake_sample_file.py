def generate_cmake_script(size_mb=1.0):
    target_bytes = size_mb * 1024 * 1024
    output = []

    current_bytes = 0
    i = 0

    while current_bytes < target_bytes:
        # Pure logic commands: set, if, foreach, list.
        # These are strictly language processing.
        block = (
            f'set(VAR_NAME_{i} "Value_{i}")\n'
            f'if("Value_{i}" STREQUAL VAR_NAME_{i})\n'
            f"  set(RESULT_{i} TRUE)\n"
            f"endif()\n"
            f"foreach(item RANGE 1 2)\n"
            f'  list(APPEND LIST_{i} "item_${{item}}")\n'
            f"endforeach()\n\n"
        )
        output.append(block)
        current_bytes += len(block)
        i += 1

    return "".join(output)


if __name__ == "__main__":
    content = generate_cmake_script(5)
    with open("pure_logic.cmake", "w") as f:
        f.write(content)
