#!/bin/sh
find . -type f \( -name "*.h" -o -name "*.h" \) -exec clang-format -i {} \;
find . -type f \( -name "*.c" -o -name "*.c" \) -exec clang-format -i {} \;