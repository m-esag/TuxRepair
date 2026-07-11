<p align="center">
  <img src="desktop-ui/app_icon.png" alt="TuxRepair Logo" width="180">
</p>

<h1 align="center">TuxRepair</h1>

<p align="center">
  <strong>Open-Source Automotive Shop Management System</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#build--install">Build</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#contributing">Contributing</a> •
  <a href="#license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B23-blue?logo=cplusplus" alt="C++23">
  <img src="https://img.shields.io/badge/UI-Qt6-41cd52?logo=qt" alt="Qt6">
  <img src="https://img.shields.io/badge/database-SQLite3-003B57?logo=sqlite" alt="SQLite3">
  <img src="https://img.shields.io/badge/license-AGPL--3.0-orange" alt="License: AGPL-3.0">
  <img src="https://img.shields.io/badge/platform-Linux-FCC624?logo=linux&logoColor=black" alt="Linux">
</p>

---

**TuxRepair** is a high-density, high-efficiency automotive shop management system built for independent repair shops. It provides a complete service counter workflow — from vehicle intake and work order creation to parts/labor billing, double-entry accounting, and PDF print output — all in a single offline-first desktop application.

No cloud subscriptions. No vendor lock-in. Your data stays on your machine in a standard SQLite database.

## Features

### 🔧 Work Orders & Service Counter
- **Rapid Vehicle Intake** — Search by license plate with live typeahead suggestions
- **Full Work Order Lifecycle** — Create Quotes, Estimates, and Invoices with one-click transitions
- **10-Row Line Items Grid** — Right-click context menus for Part/Labor type selection on each row
- **Predefined Job Kits** — One-click insertion of common service packages (Oil Change, Brake Service, etc.)
- **Catalog Lookup Dialog** — Search parts inventory and labor codes with instant insert
- **Customer & Vehicle Management** — Editable customer/vehicle details tied to each work order

### 📊 Double-Entry Accounting Ledger
- **GnuCash-Style Chart of Accounts** — Assets, Liabilities, Income, Expense accounts with dynamic balances
- **Transaction Journal** — Every finalized invoice posts proper debit/credit splits
- **Account Register View** — Click any account to see running balance history
- **Automatic COGS Tracking** — Parts cost-of-goods-sold and inventory asset adjustments on finalization

### 📦 Inventory Manager
- **Parts Catalog** — Track part numbers, descriptions, stock quantities, reorder points
- **Wholesale vs. Retail Pricing** — Maintain cost and selling prices in cents precision
- **Auto-Deduction on Finalize** — Inventory counts automatically reduce when work orders are closed

### 📅 Bay Scheduler
- **Service Bay Management** — Configurable bays (Lift 1, Lift 2, Alignment, etc.)
- **Appointment Scheduling** — Assign customers/vehicles to time slots with notes

### 🖨️ Print Templates & PDF Output
- **3 Built-in Templates** — Standard Tech Traveler, Compact Thermal Receipt, Formal Customer Invoice
- **Custom Template Editor** — HTML/CSS editor with live preview and click-to-insert placeholders
- **Smart PDF Rendering** — Automatic page size detection (Letter vs. 80mm thermal roll)
- **Barcode Generation** — SVG barcode rendered on every printed traveler

### ⚙️ Configuration
- **Configurable Tax Rate** — Set your local sales tax percentage
- **Shop Supplies Fee** — Automatic 5% of labor (min $2, max $25)
- **Light/Dark Theme Toggle** — Full theme switching with one click
- **Database Backup** — One-click SQLite backup to timestamped files

---

## Screenshots

> *Coming soon — screenshots of the Work Orders tab, Accounting Ledger, Inventory Manager, and Print Template Editor.*

---

## Build & Install

### Prerequisites

| Dependency | Version | Notes |
|---|---|---|
| **CMake** | 3.16+ | Build system generator |
| **C++ Compiler** | GCC 12+ / Clang 15+ | Must support C++23 |
| **Qt6** | 6.x | Modules: Core, Gui, Widgets, PrintSupport |
| **SQLite3** | 3.x | Development headers and library |

#### Install Dependencies (Linux)

**Fedora / OpenMandriva / RPM-based:**
```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qttools-devel sqlite-devel
```

**Ubuntu / Debian:**
```bash
sudo apt install cmake g++ qt6-base-dev libqt6printsupport6 libsqlite3-dev
```

**Arch Linux:**
```bash
sudo pacman -S cmake qt6-base sqlite
```

### Build

```bash
# 1. Clone the repository
git clone https://github.com/YOUR_USERNAME/TuxRepair.git
cd TuxRepair

# 2. Configure
cmake -B build -S .

# 3. Compile
cmake --build build -j$(nproc)

# 4. Run
./build/desktop-ui/tuxrepair-ui
```

The application creates a `tuxrepair.db` SQLite database in the working directory on first launch, pre-seeded with default accounts, sample inventory, and job kit templates.

---

## Architecture

```
TuxRepair/
├── CMakeLists.txt              # Root build configuration
├── core/                       # Core business logic library (no Qt dependency in headers)
│   ├── CMakeLists.txt
│   ├── models.h                # Data structs: Customer, Vehicle, Invoice, Account, etc.
│   ├── db_manager.h            # SQLite database manager interface
│   └── db_manager.cpp          # Schema, CRUD, transactions, accounting splits
├── desktop-ui/                 # Qt6 desktop application
│   ├── CMakeLists.txt
│   ├── main.cpp                # Entry point, DB initialization, demo data seeding
│   ├── main_window.cpp/h       # Primary UI: tabs, menus, work order workflow
│   ├── traveler_renderer.cpp/h # HTML template engine + QPrinter PDF renderer
│   ├── qol_dialogs.cpp/h       # Catalog lookup, customer search, service history
│   ├── register_dialog.cpp/h   # Account register drill-down dialog
│   ├── template_editor_dialog.cpp/h  # Print template HTML/CSS editor with live preview
│   ├── resources.qrc           # Qt resource file (app icon)
│   └── app_icon.png            # Application icon
├── LICENSE                     # GNU AGPL v3
└── .gitignore
```

### Key Design Decisions

- **Offline-First** — All data stored locally in SQLite. No network required, no SaaS dependency.
- **Cents-Based Currency** — All monetary values stored as `int64_t` cents to avoid floating-point rounding errors in financial calculations.
- **Double-Entry Accounting** — Every finalized invoice creates proper balanced debit/credit transaction splits across the chart of accounts.
- **Thread-Safe Database** — All `DBManager` methods protected by `std::recursive_mutex` for safe concurrent access.
- **Template Placeholders** — Print templates use `{{PLACEHOLDER}}` syntax (e.g., `{{CUSTOMER_NAME}}`, `{{TICKET_ITEMS_TABLE}}`) replaced at render time.

---

## Database Schema

TuxRepair uses the following SQLite tables:

| Table | Purpose |
|---|---|
| `customers` | Customer records (name, address, phone) |
| `vehicles` | Vehicles linked to customers (plate, VIN, year, model, engine) |
| `invoices` | Work orders / tickets (type, mileage, status, dates) |
| `invoice_items` | Line items on each invoice (parts and labor) |
| `accounts` | Chart of accounts (Asset, Liability, Income, Expense) |
| `transactions` | Accounting transaction headers |
| `splits` | Double-entry splits (debits positive, credits negative) |
| `inventory` | Parts inventory with stock tracking |
| `job_kits` / `job_kit_items` | Predefined service packages |
| `bays` / `bay_schedules` | Service bay definitions and appointments |
| `print_templates` | Custom HTML print templates |
| `settings` | Key-value application settings (tax rate, etc.) |

---

## Contributing

Contributions are welcome! This is an open-source project licensed under the GNU AGPL v3.

1. **Fork** the repository
2. **Create a feature branch**: `git checkout -b feature/my-feature`
3. **Commit your changes**: `git commit -m "Add my feature"`
4. **Push to the branch**: `git push origin feature/my-feature`
5. **Open a Pull Request**

### Development Guidelines
- Follow the existing code style (4-space indentation, `snake_case` for variables/functions)
- All monetary values must be stored as `int64_t` cents — never floating-point
- Add null guards for all `sqlite3_column_text()` calls
- Use `std::recursive_mutex` lock guards in all `DBManager` public methods
- Test with both light and dark themes

---

## Roadmap

- [ ] Customer/vehicle creation directly from work order panel
- [ ] Search by VIN, name, or phone number (not just license plate)
- [ ] Export accounting data to CSV/OFX format
- [ ] Multi-technician assignment per line item with payroll tracking
- [ ] Dashboard tab with daily KPIs and low-stock alerts
- [ ] Auto-save timer for work orders
- [ ] Undo/redo stack for line item edits
- [ ] Keyboard shortcut help overlay

---

## License

TuxRepair is licensed under the **GNU Affero General Public License v3.0** (AGPL-3.0).

See [LICENSE](LICENSE) for the full license text.

---

<p align="center">
  <sub>Built with ❤️ for independent repair shops everywhere.</sub>
</p>
