# mini-calc

A small GTK-3 based C calculator that functions as both a CLI tool and a graphical application.

## Features

- **Dual Interface:** Use it from the command line for quick calculations or the GUI for standard desktop use.
- **Scientific Support:** Handles basic arithmetic, parentheses, `sqrt()`, and constants like `PI` and `E`.
- **Theming:** Includes integrated support for both Light and Dark modes via CSS styling.
- **Robustness:** Built with strict safety standards to ensure reliability.

## Build & Security

The project emphasizes code quality and security. The `Makefile` includes extensive compiler flags (`-Wall`, `-Wextra`, `-pedantic`, etc.) and modern hardening techniques (`FORTIFY_SOURCE`, `stack-protector`, `PIE`).

### Build Options

- `make all`: Standard clean build with full warnings and hardening.
- `make asan`: Builds the application with AddressSanitizer (`-fsanitize=address`) and debug symbols. Useful for checking memory safety and leaks. Run with `ASAN_OPTIONS=detect_leaks=1 ./calc`.
- `make test`: Runs a suite of CLI-based calculations to verify core functionality.
- `make format`: Automatically format the code using `clang-format`.
- `make lint`: Run static analysis using `clang`.

## License

This project is licensed under the MIT License.
