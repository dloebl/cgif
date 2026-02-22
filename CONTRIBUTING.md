# Contributing to cgif

Thank you for your interest in contributing to cgif! We welcome contributions from everyone.

**Important:** Please note that pull requests or bug reports that do not meet the requirements described below will be closed.

## Reporting Bugs

If you find a bug, please open an issue on GitHub. When reporting a bug, please include:

1.  A clear description of the issue.
2.  Steps to reproduce the bug.
3.  **Mandatory:** A minimal test case that reproduces the issue. This should ideally be a small C program similar to the existing tests in the `tests/` directory.
4.  **Proof of Failure:** Paste the console output showing the test failing on the `main` branch. This confirms that the issue is reproducible.

Bug reports that do not include a reproduction test case will be closed.

## Submitting Pull Requests

1.  **Fork the repository** and create a new branch for your feature or bug fix.
2.  **Keep it Focused:**
    *   Do not include unrelated refactoring, whitespace changes, or formatting fixes in the same PR as a bug fix or feature.
    *   PRs with excessive "noise" or unrelated changes will be closed.
3.  **Write Tests:**
    *   For bug fixes, you **MUST** include a test case that reproduces the issue on the `main` branch (fails before your fix) and passes with your fix.
    *   For new features, add tests that cover the new functionality.
    *   Add your new test C file to `tests/` and register it in `tests/meson.build`.
4.  **Run Tests:** Ensure all tests pass before submitting.
    ```bash
    meson setup build/
    meson test -C build/
    ```
5.  **AI Tooling Attribution:** If you used any AI models or tooling (e.g., ChatGPT, Claude Code, GitHub Copilot, Gemini, etc.) to create your PR, please specify:
    *   The model used (e.g., "Claude Sonnet 4.6", "Gemini 3 Pro", "GPT-5.2").
    *   A brief description of how it was used (e.g., "Used to generate the initial implementation of function X", "Used to write test cases").
    *   This helps us understand the provenance of the code.

Pull requests that do not meet these requirements (including failing tests or missing reproduction cases) will be closed.

## Development Workflow

### Building
The project uses the Meson build system.

```bash
meson setup build/
```

### Testing
To run the test suite:

```bash
meson test -C build/
```

## Coding Style

*   This project follows the C99 standard.
*   Please adhere to the existing coding style in the project (indentation, naming conventions, etc.).
*   Avoid adding unnecessary dependencies.

## License

By contributing to cgif, you agree that your contributions will be licensed under the MIT License, as defined in the `LICENSE` file.
