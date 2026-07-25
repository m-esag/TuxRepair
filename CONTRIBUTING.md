# Contributing to TuxRepair

Thank you for your interest in contributing to TuxRepair! This guide will help you get started.

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/m-esag/TuxRepair.git
   cd TuxRepair
   ```
3. **Create a branch** for your work:
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. **Build** using one of the CMake presets:
   ```bash
   # Debug build with ASan + UBSan (recommended for development)
   cmake --preset debug
   cmake --build --preset debug -j$(nproc)

   # Optimized release build
   cmake --preset release
   cmake --build --preset release -j$(nproc)

   # Run the app
   ./build-debug/desktop-ui/tuxrepair-ui
   ```
5. **Run the tests** before opening a PR:
   ```bash
   cmake --build --preset debug
   ctest --preset debug
   ```

## Code Style

- **Indentation**: 4 spaces (no tabs)
- **Naming**: `snake_case` for variables, functions, and file names
- **Member variables**: Prefix with `m_` (e.g., `m_active_invoice_id`)
- **Namespaces**: All code lives under the `tuxrepair` namespace
- **Braces**: Same-line opening braces for functions and control structures
- **Includes**: Group by category (project headers, Qt headers, standard library)

## Financial Data Guidelines

- **All monetary values** must be stored as `int64_t` in **cents** (e.g., `$12.50` = `1250`)
- **Never** use `float` or `double` for stored currency values
- Display formatting should convert cents → dollars only at the UI layer

## Database Guidelines

- All `sqlite3_column_text()` calls **must** have null pointer guards:
  ```cpp
  const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  field = val ? val : "";
  ```
- Use `std::lock_guard<std::recursive_mutex>` in all public `DBManager` methods
- Wrap multi-step writes in `BEGIN TRANSACTION` / `COMMIT` / `ROLLBACK`
- Use parameterized queries (`?` bindings) — never string concatenation for values

## Pull Request Process

1. Ensure the project **builds without warnings** on your platform
2. Test both **light and dark themes** for any UI changes
3. Update the **README** if you add new features or change behavior
4. Write a clear PR description explaining **what** and **why**

## Known Gaps

- **Car catalog import script (`import_car_db.py` + the Teoalida sample SQL) is currently gitignored** and therefore unavailable to contributors after a fresh clone. The on-screen vehicle-catalog features still work off whatever data the local `tuxrepair.db` already contains, but the import path described in the README is not reproducible from the public repo today. **TODO(phase-6):** decide whether to ship the script + sample (preferred — possibly via Git LFS or a release artifact, since the sample is ~3.9 MB) or remove the car-catalog feature claims from the README.

## Reporting Bugs

Open an issue with:
- Steps to reproduce the problem
- Expected vs. actual behavior
- Your OS, Qt version, and compiler version
- Console output (stderr) if applicable

## Feature Requests

Open an issue with the `enhancement` label describing:
- The use case or workflow you want to improve
- How you envision the feature working
- Any mockups or examples from similar software

## License

By contributing, you agree that your contributions will be licensed under the **GNU AGPL v3.0** license.
