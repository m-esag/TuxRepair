#include "db_manager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace tuxrepair {

namespace {
std::string safe_col_text(sqlite3_stmt* stmt, int col) {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? text : "";
}
}

DBManager::DBManager() : m_db(nullptr) {}

DBManager::~DBManager() {
    close();
}

bool DBManager::open(const std::string& db_path) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (sqlite3_open(db_path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    // Enable foreign keys
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    return initializeSchema();
}

void DBManager::close() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool DBManager::executeSQL(const std::string& sql) {
    char* err_msg = nullptr;
    if (sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool DBManager::initializeSchema() {
    std::string sql_schema = 
        "CREATE TABLE IF NOT EXISTS customers ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  last_name TEXT NOT NULL,"
        "  first_name TEXT NOT NULL,"
        "  middle_name TEXT,"
        "  address TEXT,"
        "  city TEXT,"
        "  phone_number TEXT"
        ");"
        
        "CREATE TABLE IF NOT EXISTS vehicles ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  customer_id INTEGER NOT NULL,"
        "  license_plate TEXT NOT NULL,"
        "  vin TEXT,"
        "  year INTEGER,"
        "  model TEXT,"
        "  engine_specs TEXT,"
        "  FOREIGN KEY(customer_id) REFERENCES customers(id)"
        ");"
        
        "CREATE INDEX IF NOT EXISTS idx_vehicles_plate ON vehicles(license_plate);"
        
        "CREATE TABLE IF NOT EXISTS invoices ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  customer_id INTEGER NOT NULL,"
        "  vehicle_id INTEGER NOT NULL,"
        "  ticket_type TEXT NOT NULL,"
        "  mileage_in INTEGER,"
        "  mileage_out INTEGER,"
        "  date_created TEXT,"
        "  status TEXT,"
        "  supplies_removed INTEGER DEFAULT 0,"
        "  writer TEXT DEFAULT 'Office',"
        "  FOREIGN KEY(customer_id) REFERENCES customers(id),"
        "  FOREIGN KEY(vehicle_id) REFERENCES vehicles(id)"
        ");"
        
        "CREATE TABLE IF NOT EXISTS invoice_items ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  invoice_id INTEGER NOT NULL,"
        "  part_number TEXT NOT NULL,"
        "  description TEXT NOT NULL,"
        "  quantity REAL NOT NULL,"
        "  unit_price INTEGER NOT NULL,"
        "  specification TEXT NOT NULL,"
        "  FOREIGN KEY(invoice_id) REFERENCES invoices(id) ON DELETE CASCADE"
        ");"
        
        "CREATE TABLE IF NOT EXISTS accounts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  type TEXT NOT NULL"
        ");"
        
        "CREATE TABLE IF NOT EXISTS transactions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  date TEXT NOT NULL,"
        "  description TEXT"
        ");"
        
        "CREATE TABLE IF NOT EXISTS splits ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  transaction_id INTEGER NOT NULL,"
        "  account_id INTEGER NOT NULL,"
        "  amount INTEGER NOT NULL,"
        "  FOREIGN KEY(transaction_id) REFERENCES transactions(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(account_id) REFERENCES accounts(id)"
        ");"
        
        "CREATE TABLE IF NOT EXISTS inventory ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  part_number TEXT NOT NULL UNIQUE,"
        "  description TEXT NOT NULL,"
        "  quantity_on_hand REAL NOT NULL DEFAULT 0.0,"
        "  reorder_point REAL NOT NULL DEFAULT 5.0,"
        "  wholesale_cost INTEGER NOT NULL DEFAULT 0,"
        "  retail_price INTEGER NOT NULL DEFAULT 0"
        ");"
        
        "CREATE TABLE IF NOT EXISTS job_kits ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  description TEXT"
        ");"
        
        "CREATE TABLE IF NOT EXISTS job_kit_items ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  kit_id INTEGER NOT NULL,"
        "  item_type TEXT NOT NULL,"
        "  description TEXT NOT NULL,"
        "  quantity REAL NOT NULL DEFAULT 1.0,"
        "  unit_price INTEGER NOT NULL DEFAULT 0,"
        "  FOREIGN KEY(kit_id) REFERENCES job_kits(id) ON DELETE CASCADE"
        ");"
        
        "CREATE TABLE IF NOT EXISTS bays ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE"
        ");"
        
        "CREATE TABLE IF NOT EXISTS bay_schedules ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  bay_id INTEGER NOT NULL,"
        "  invoice_id INTEGER,"
        "  customer_name TEXT NOT NULL,"
        "  vehicle_info TEXT NOT NULL,"
        "  time_slot TEXT NOT NULL,"
        "  notes TEXT,"
        "  FOREIGN KEY(bay_id) REFERENCES bays(id) ON DELETE CASCADE"
        ");"
        
        "CREATE TABLE IF NOT EXISTS print_templates ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  content_html TEXT NOT NULL,"
        "  is_active INTEGER NOT NULL DEFAULT 0"
        ");"

        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY NOT NULL,"
        "  value TEXT NOT NULL"
        ");";

    if (!executeSQL(sql_schema)) return false;

    // Ensure supplies_removed column exists in invoices table for backward compatibility
    executeSQL("ALTER TABLE invoices ADD COLUMN supplies_removed INTEGER DEFAULT 0;");
    executeSQL("ALTER TABLE invoices ADD COLUMN writer TEXT DEFAULT 'Office';");

    // Ensure item_type column is renamed to part_number in invoice_items for backward compatibility
    executeSQL("ALTER TABLE invoice_items RENAME COLUMN item_type TO part_number;");

    // Ensure specification column exists in invoice_items table for backward compatibility
    executeSQL("ALTER TABLE invoice_items ADD COLUMN specification TEXT NOT NULL DEFAULT 'Part';");

    // Seed default accounts, bays, job kits, and inventory
    std::string seed_data = 
        "INSERT OR IGNORE INTO accounts (name, type) VALUES ('Checking Asset', 'Asset');"
        "INSERT OR IGNORE INTO accounts (name, type) VALUES ('Parts Inventory Asset', 'Asset');"
        "INSERT OR IGNORE INTO accounts (name, type) VALUES ('Parts Income', 'Income');"
        "INSERT OR IGNORE INTO accounts (name, type) VALUES ('Labor Income', 'Income');"
        "INSERT OR IGNORE INTO accounts (name, type) VALUES ('Cost of Goods Sold Expense', 'Expense');"
        "INSERT OR IGNORE INTO accounts (name, type) VALUES ('Sales Tax Liability', 'Liability');"
        
        "INSERT OR IGNORE INTO bays (name) VALUES ('Bay 1 - Lift 1');"
        "INSERT OR IGNORE INTO bays (name) VALUES ('Bay 2 - Lift 2');"
        "INSERT OR IGNORE INTO bays (name) VALUES ('Bay 3 - Alignment');"
        
        "INSERT OR IGNORE INTO inventory (part_number, description, quantity_on_hand, reorder_point, wholesale_cost, retail_price) VALUES ('5W30-SB', '5W-30 Synthetic Blend Oil (Quart)', 24.0, 10.0, 250, 500);"
        "INSERT OR IGNORE INTO inventory (part_number, description, quantity_on_hand, reorder_point, wholesale_cost, retail_price) VALUES ('OF-PREM', 'Premium Spin-on Oil Filter', 4.0, 5.0, 300, 800);" // Note: 4.0 is below reorder point 5.0!
        "INSERT OR IGNORE INTO inventory (part_number, description, quantity_on_hand, reorder_point, wholesale_cost, retail_price) VALUES ('BP-FRONT', 'Front Ceramic Brake Pads Set', 3.0, 2.0, 1500, 3500);"
        
        "INSERT OR IGNORE INTO job_kits (id, name, description) VALUES (1, 'Premium Oil Change Service', 'Includes 5 quarts oil, premium filter, and chassis lube labor');"
        "INSERT OR IGNORE INTO job_kit_items (kit_id, item_type, description, quantity, unit_price) VALUES (1, 'Part', '5W-30 Synthetic Blend Oil (Quart)', 5.0, 500);"
        "INSERT OR IGNORE INTO job_kit_items (kit_id, item_type, description, quantity, unit_price) VALUES (1, 'Part', 'Premium Spin-on Oil Filter', 1.0, 800);"
        "INSERT OR IGNORE INTO job_kit_items (kit_id, item_type, description, quantity, unit_price) VALUES (1, 'Labor', 'Standard Lube, Oil & Filter Labor', 1.0, 2500);"
        
        "INSERT OR IGNORE INTO job_kits (id, name, description) VALUES (2, 'Front Brake Pad Replacement', 'Includes ceramic front pads and labor to install');"
        "INSERT OR IGNORE INTO job_kit_items (kit_id, item_type, description, quantity, unit_price) VALUES (2, 'Part', 'Front Ceramic Brake Pads Set', 1.0, 3500);"
        "INSERT OR IGNORE INTO job_kit_items (kit_id, item_type, description, quantity, unit_price) VALUES (2, 'Labor', 'Front Disc Brake Service Labor', 1.0, 6500);";

    bool res = executeSQL(seed_data);
    if (res) {
        seedDefaultTemplates();
    }
    // Check columns of invoice_items
    std::string check_sql = "PRAGMA table_info(invoice_items);";
    sqlite3_stmt* stmt_chk = nullptr;
    if (sqlite3_prepare_v2(m_db, check_sql.c_str(), -1, &stmt_chk, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt_chk) == SQLITE_ROW) {
            const char* col_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt_chk, 1));
            std::cerr << "invoice_items column: " << (col_name ? col_name : "NULL") << std::endl;
        }
        sqlite3_finalize(stmt_chk);
    }

    return res;
}

int DBManager::insertCustomer(const Customer& customer) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "INSERT INTO customers (last_name, first_name, middle_name, address, city, phone_number) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, customer.last_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, customer.first_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, customer.middle_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, customer.address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, customer.city.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, customer.phone_number.c_str(), -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }
    sqlite3_finalize(stmt);
    return id;
}

bool DBManager::getCustomer(int id, Customer& out_customer) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "SELECT id, last_name, first_name, middle_name, address, city, phone_number FROM customers WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_customer.id = sqlite3_column_int(stmt, 0);
        out_customer.last_name = safe_col_text(stmt, 1);
        out_customer.first_name = safe_col_text(stmt, 2);
        const char* mid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out_customer.middle_name = mid ? mid : "";
        const char* addr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        out_customer.address = addr ? addr : "";
        const char* city = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        out_customer.city = city ? city : "";
        const char* phone = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        out_customer.phone_number = phone ? phone : "";
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool DBManager::updateCustomer(const Customer& customer) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "UPDATE customers SET last_name = ?, first_name = ?, middle_name = ?, address = ?, city = ?, phone_number = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, customer.last_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, customer.first_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, customer.middle_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, customer.address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, customer.city.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, customer.phone_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, customer.id);

    int rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    if (!ok) {
        std::cerr << "updateCustomer sqlite3_step failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
    }
    sqlite3_finalize(stmt);
    return ok;
}

int DBManager::insertVehicle(const Vehicle& vehicle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "INSERT INTO vehicles (customer_id, license_plate, vin, year, model, engine_specs) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, vehicle.customer_id);
    sqlite3_bind_text(stmt, 2, vehicle.license_plate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, vehicle.vin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, vehicle.year);
    sqlite3_bind_text(stmt, 5, vehicle.model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, vehicle.engine_specs.c_str(), -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }
    sqlite3_finalize(stmt);
    return id;
}

bool DBManager::getVehicle(int id, Vehicle& out_vehicle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "SELECT id, customer_id, license_plate, vin, year, model, engine_specs FROM vehicles WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_vehicle.id = sqlite3_column_int(stmt, 0);
        out_vehicle.customer_id = sqlite3_column_int(stmt, 1);
        out_vehicle.license_plate = safe_col_text(stmt, 2);
        const char* vin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out_vehicle.vin = vin ? vin : "";
        out_vehicle.year = sqlite3_column_int(stmt, 4);
        out_vehicle.model = safe_col_text(stmt, 5);
        const char* eng = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        out_vehicle.engine_specs = eng ? eng : "";
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

std::vector<Vehicle> DBManager::searchVehiclesByPlate(const std::string& plate) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<Vehicle> results;
    std::string sql = "SELECT id, customer_id, license_plate, vin, year, model, engine_specs FROM vehicles WHERE license_plate LIKE ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

    std::string wild = plate + "%";
    sqlite3_bind_text(stmt, 1, wild.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Vehicle v;
        v.id = sqlite3_column_int(stmt, 0);
        v.customer_id = sqlite3_column_int(stmt, 1);
        v.license_plate = safe_col_text(stmt, 2);
        const char* vin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        v.vin = vin ? vin : "";
        v.year = sqlite3_column_int(stmt, 4);
        v.model = safe_col_text(stmt, 5);
        const char* eng = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        v.engine_specs = eng ? eng : "";
        results.push_back(v);
    }
    sqlite3_finalize(stmt);
    return results;
}

bool DBManager::updateVehicle(const Vehicle& vehicle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "UPDATE vehicles SET license_plate = ?, vin = ?, year = ?, model = ?, engine_specs = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, vehicle.license_plate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, vehicle.vin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, vehicle.year);
    sqlite3_bind_text(stmt, 4, vehicle.model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, vehicle.engine_specs.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, vehicle.id);

    int rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    if (!ok) {
        std::cerr << "updateVehicle sqlite3_step failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
    }
    sqlite3_finalize(stmt);
    return ok;
}

int DBManager::createInvoice(int customer_id, int vehicle_id, const std::string& ticket_type, int mileage_in, const std::string& date_created) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "INSERT INTO invoices (customer_id, vehicle_id, ticket_type, mileage_in, mileage_out, date_created, status) VALUES (?, ?, ?, ?, 0, ?, 'Open');";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, customer_id);
    sqlite3_bind_int(stmt, 2, vehicle_id);
    sqlite3_bind_text(stmt, 3, ticket_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, mileage_in);
    sqlite3_bind_text(stmt, 5, date_created.c_str(), -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }
    sqlite3_finalize(stmt);
    return id;
}

bool DBManager::getInvoice(int id, Invoice& out_invoice) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "SELECT id, customer_id, vehicle_id, ticket_type, mileage_in, mileage_out, date_created, status, supplies_removed, writer FROM invoices WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_invoice.id = sqlite3_column_int(stmt, 0);
        out_invoice.customer_id = sqlite3_column_int(stmt, 1);
        out_invoice.vehicle_id = sqlite3_column_int(stmt, 2);
        const char* ttype = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out_invoice.ticket_type = ttype ? ttype : "";
        out_invoice.mileage_in = sqlite3_column_int(stmt, 4);
        out_invoice.mileage_out = sqlite3_column_int(stmt, 5);
        const char* dcreated = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        out_invoice.date_created = dcreated ? dcreated : "";
        const char* stat = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        out_invoice.status = stat ? stat : "";
        out_invoice.supplies_removed = sqlite3_column_int(stmt, 8) != 0;
        const char* wr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        out_invoice.writer = wr ? wr : "Office";
        found = true;
    }
    sqlite3_finalize(stmt);

    if (!found) return false;

    // Fetch customer details
    {
        std::string sql_c = "SELECT id, last_name, first_name, middle_name, address, city, phone_number FROM customers WHERE id = ?;";
        sqlite3_stmt* stmt_c = nullptr;
        if (sqlite3_prepare_v2(m_db, sql_c.c_str(), -1, &stmt_c, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt_c, 1, out_invoice.customer_id);
            if (sqlite3_step(stmt_c) == SQLITE_ROW) {
                out_invoice.customer.id = sqlite3_column_int(stmt_c, 0);
                const char* ln = reinterpret_cast<const char*>(sqlite3_column_text(stmt_c, 1));
                out_invoice.customer.last_name = ln ? ln : "";
                const char* fn = reinterpret_cast<const char*>(sqlite3_column_text(stmt_c, 2));
                out_invoice.customer.first_name = fn ? fn : "";
                const char* mid = reinterpret_cast<const char*>(sqlite3_column_text(stmt_c, 3));
                out_invoice.customer.middle_name = mid ? mid : "";
                const char* addr = reinterpret_cast<const char*>(sqlite3_column_text(stmt_c, 4));
                out_invoice.customer.address = addr ? addr : "";
                const char* city = reinterpret_cast<const char*>(sqlite3_column_text(stmt_c, 5));
                out_invoice.customer.city = city ? city : "";
                const char* phone = reinterpret_cast<const char*>(sqlite3_column_text(stmt_c, 6));
                out_invoice.customer.phone_number = phone ? phone : "";
            }
            sqlite3_finalize(stmt_c);
        }
    }

    // Fetch vehicle details
    {
        std::string sql_v = "SELECT id, customer_id, license_plate, vin, year, model, engine_specs FROM vehicles WHERE id = ?;";
        sqlite3_stmt* stmt_v = nullptr;
        if (sqlite3_prepare_v2(m_db, sql_v.c_str(), -1, &stmt_v, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt_v, 1, out_invoice.vehicle_id);
            if (sqlite3_step(stmt_v) == SQLITE_ROW) {
                out_invoice.vehicle.id = sqlite3_column_int(stmt_v, 0);
                out_invoice.vehicle.customer_id = sqlite3_column_int(stmt_v, 1);
                const char* vplate = reinterpret_cast<const char*>(sqlite3_column_text(stmt_v, 2));
                out_invoice.vehicle.license_plate = vplate ? vplate : "";
                const char* vin = reinterpret_cast<const char*>(sqlite3_column_text(stmt_v, 3));
                out_invoice.vehicle.vin = vin ? vin : "";
                out_invoice.vehicle.year = sqlite3_column_int(stmt_v, 4);
                const char* vmod = reinterpret_cast<const char*>(sqlite3_column_text(stmt_v, 5));
                out_invoice.vehicle.model = vmod ? vmod : "";
                const char* eng = reinterpret_cast<const char*>(sqlite3_column_text(stmt_v, 6));
                out_invoice.vehicle.engine_specs = eng ? eng : "";
            }
            sqlite3_finalize(stmt_v);
        }
    }

    // Fetch invoice items
    {
        out_invoice.items.clear();
        std::string sql_i = "SELECT id, invoice_id, part_number, description, quantity, unit_price, specification FROM invoice_items WHERE invoice_id = ?;";
        sqlite3_stmt* stmt_i = nullptr;
        if (sqlite3_prepare_v2(m_db, sql_i.c_str(), -1, &stmt_i, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt_i, 1, id);
            while (sqlite3_step(stmt_i) == SQLITE_ROW) {
                InvoiceItem item;
                item.id = sqlite3_column_int(stmt_i, 0);
                item.invoice_id = sqlite3_column_int(stmt_i, 1);
                const char* pn = reinterpret_cast<const char*>(sqlite3_column_text(stmt_i, 2));
                item.part_number = pn ? pn : "";
                const char* ds = reinterpret_cast<const char*>(sqlite3_column_text(stmt_i, 3));
                item.description = ds ? ds : "";
                item.quantity = sqlite3_column_double(stmt_i, 4);
                item.unit_price = sqlite3_column_int64(stmt_i, 5);
                const char* sp = reinterpret_cast<const char*>(sqlite3_column_text(stmt_i, 6));
                item.specification = sp ? sp : "";
                out_invoice.items.push_back(item);
            }
            sqlite3_finalize(stmt_i);
        }
    }

    return true;
}

std::vector<Invoice> DBManager::getAllInvoices() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<Invoice> list;
    std::string sql = "SELECT id FROM invoices ORDER BY id DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return list;

    std::vector<int> ids;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // Since we now use std::recursive_mutex, we can safely call getInvoice directly.
    for (int id : ids) {
        Invoice inv;
        if (getInvoice(id, inv)) {
            list.push_back(inv);
        }
    }
    return list;
}

bool DBManager::updateInvoiceHeader(int id, const std::string& ticket_type, int mileage_in, int mileage_out, const std::string& status, bool supplies_removed, const std::string& writer) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "UPDATE invoices SET ticket_type = ?, mileage_in = ?, mileage_out = ?, status = ?, supplies_removed = ?, writer = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, ticket_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, mileage_in);
    sqlite3_bind_int(stmt, 3, mileage_out);
    sqlite3_bind_text(stmt, 4, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, supplies_removed ? 1 : 0);
    sqlite3_bind_text(stmt, 6, writer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DBManager::updateInvoiceVehicle(int invoice_id, int vehicle_id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "UPDATE invoices SET vehicle_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, vehicle_id);
    sqlite3_bind_int(stmt, 2, invoice_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DBManager::saveInvoiceItems(int invoice_id, const std::vector<InvoiceItem>& items) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int rc;
    
    rc = sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "saveInvoiceItems BEGIN TRANSACTION failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
    }

    // Delete existing
    std::string del_sql = "DELETE FROM invoice_items WHERE invoice_id = ?;";
    sqlite3_stmt* del_stmt = nullptr;
    rc = sqlite3_prepare_v2(m_db, del_sql.c_str(), -1, &del_stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "saveInvoiceItems prepare DELETE failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int(del_stmt, 1, invoice_id);
    rc = sqlite3_step(del_stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        std::cerr << "saveInvoiceItems step DELETE failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
    }
    sqlite3_finalize(del_stmt);

    // Insert new
    std::string ins_sql = "INSERT INTO invoice_items (invoice_id, part_number, description, quantity, unit_price, specification) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* ins_stmt = nullptr;
    rc = sqlite3_prepare_v2(m_db, ins_sql.c_str(), -1, &ins_stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "saveInvoiceItems prepare INSERT failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& item : items) {
        sqlite3_reset(ins_stmt);
        sqlite3_bind_int(ins_stmt, 1, invoice_id);
        sqlite3_bind_text(ins_stmt, 2, item.part_number.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins_stmt, 3, item.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(ins_stmt, 4, item.quantity);
        sqlite3_bind_int64(ins_stmt, 5, item.unit_price);
        sqlite3_bind_text(ins_stmt, 6, item.specification.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(ins_stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "saveInvoiceItems step INSERT failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
            sqlite3_finalize(ins_stmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
    }
    sqlite3_finalize(ins_stmt);
    rc = sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "saveInvoiceItems COMMIT failed: " << sqlite3_errmsg(m_db) << " (code: " << rc << ")" << std::endl;
        return false;
    }
    return true;
}

int DBManager::getAccountIdByName(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "SELECT id FROM accounts WHERE name = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

bool DBManager::finalizeInvoice(int invoice_id, int64_t parts_cost_cents, double tax_rate, bool supplies_removed) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Calculate totals of invoice items
    int64_t parts_retail_total = 0;
    int64_t labor_total = 0;

    std::string sql_items = "SELECT specification, quantity, unit_price FROM invoice_items WHERE invoice_id = ?;";
    sqlite3_stmt* stmt_items = nullptr;
    if (sqlite3_prepare_v2(m_db, sql_items.c_str(), -1, &stmt_items, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt_items, 1, invoice_id);

    while (sqlite3_step(stmt_items) == SQLITE_ROW) {
        const char* spec_raw = reinterpret_cast<const char*>(sqlite3_column_text(stmt_items, 0));
        std::string spec = spec_raw ? spec_raw : "";
        double qty = sqlite3_column_double(stmt_items, 1);
        int64_t price = sqlite3_column_int64(stmt_items, 2);
        int64_t total = static_cast<int64_t>(qty * price);
        
        std::string spec_lower = spec;
        spec_lower.erase(0, spec_lower.find_first_not_of(" \t\r\n"));
        spec_lower.erase(spec_lower.find_last_not_of(" \t\r\n") + 1);
        std::transform(spec_lower.begin(), spec_lower.end(), spec_lower.begin(), ::tolower);
        
        if (spec_lower == "part") {
            parts_retail_total += total;
        } else {
            labor_total += total;
        }
    }
    sqlite3_finalize(stmt_items);

    // Calculate tax on parts retail using user-configured rate
    int64_t tax_liability = static_cast<int64_t>(parts_retail_total * tax_rate);
    
    // Calculate shop supplies (5% of labor, min $2.00, max $25.00)
    int64_t shop_supplies = 0;
    if (labor_total > 0 && !supplies_removed) {
        shop_supplies = static_cast<int64_t>(labor_total * 0.05);
        if (shop_supplies < 200) shop_supplies = 200;
        if (shop_supplies > 2500) shop_supplies = 2500;
    }
    
    int64_t invoice_total = parts_retail_total + labor_total + shop_supplies + tax_liability;

    // Get Account IDs
    int acct_checking = getAccountIdByName("Checking Asset");
    int acct_parts_inv = getAccountIdByName("Parts Inventory Asset");
    int acct_parts_inc = getAccountIdByName("Parts Income");
    int acct_labor_inc = getAccountIdByName("Labor Income");
    int acct_cogs = getAccountIdByName("Cost of Goods Sold Expense");
    int acct_tax = getAccountIdByName("Sales Tax Liability");

    if (acct_checking == -1 || acct_parts_inv == -1 || acct_parts_inc == -1 ||
        acct_labor_inc == -1 || acct_cogs == -1 || acct_tax == -1) {
        std::cerr << "Required accounting accounts are missing!" << std::endl;
        return false;
    }

    // Begin transaction
    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // Update status to 'Finalized', ticket_type to 'Invoice', and supplies_removed
    std::string sql_upd = "UPDATE invoices SET ticket_type = 'Invoice', status = 'Finalized', supplies_removed = ? WHERE id = ?;";
    sqlite3_stmt* stmt_upd = nullptr;
    if (sqlite3_prepare_v2(m_db, sql_upd.c_str(), -1, &stmt_upd, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int(stmt_upd, 1, supplies_removed ? 1 : 0);
    sqlite3_bind_int(stmt_upd, 2, invoice_id);
    sqlite3_step(stmt_upd);
    sqlite3_finalize(stmt_upd);

    // Create Transaction
    std::string sql_tx = "INSERT INTO transactions (date, description) VALUES (date('now'), ?);";
    sqlite3_stmt* stmt_tx = nullptr;
    if (sqlite3_prepare_v2(m_db, sql_tx.c_str(), -1, &stmt_tx, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    std::string desc = "Finalized Invoice #" + std::to_string(invoice_id);
    sqlite3_bind_text(stmt_tx, 1, desc.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt_tx) != SQLITE_DONE) {
        sqlite3_finalize(stmt_tx);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    int tx_id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    sqlite3_finalize(stmt_tx);

    // Helper for split insert
    auto insert_split = [&](int account_id, int64_t amount) -> bool {
        std::string sql_sp = "INSERT INTO splits (transaction_id, account_id, amount) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt_sp = nullptr;
        if (sqlite3_prepare_v2(m_db, sql_sp.c_str(), -1, &stmt_sp, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt_sp, 1, tx_id);
        sqlite3_bind_int(stmt_sp, 2, account_id);
        sqlite3_bind_int64(stmt_sp, 3, amount);
        bool ok = (sqlite3_step(stmt_sp) == SQLITE_DONE);
        sqlite3_finalize(stmt_sp);
        return ok;
    };

    // Splits:
    // 1. Checking Asset (Debit / Positive)
    if (!insert_split(acct_checking, invoice_total)) goto rollback;
    // 2. Parts Income (Credit / Negative)
    if (!insert_split(acct_parts_inc, -parts_retail_total)) goto rollback;
    // 3. Labor Income (Credit / Negative) (Includes shop supplies charges)
    if (!insert_split(acct_labor_inc, -(labor_total + shop_supplies))) goto rollback;
    // 4. Sales Tax Liability (Credit / Negative)
    if (!insert_split(acct_tax, -tax_liability)) goto rollback;
    // 5. Cost of Goods Sold Expense (Debit / Positive)
    if (!insert_split(acct_cogs, parts_cost_cents)) goto rollback;
    // 6. Parts Inventory Asset (Credit / Negative)
    if (!insert_split(acct_parts_inv, -parts_cost_cents)) goto rollback;

    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;

rollback:
    sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return false;
}

std::vector<Account> DBManager::getAccounts() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<Account> list;
    std::string sql = "SELECT id, name, type FROM accounts;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return list;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Account a;
        a.id = sqlite3_column_int(stmt, 0);
        a.name = safe_col_text(stmt, 1);
        a.type = safe_col_text(stmt, 2);
        
        // Calculate balance dynamically
        std::string sql_bal = "SELECT SUM(amount) FROM splits WHERE account_id = ?;";
        sqlite3_stmt* stmt_bal = nullptr;
        a.balance = 0;
        if (sqlite3_prepare_v2(m_db, sql_bal.c_str(), -1, &stmt_bal, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt_bal, 1, a.id);
            if (sqlite3_step(stmt_bal) == SQLITE_ROW) {
                a.balance = sqlite3_column_int64(stmt_bal, 0);
            }
            sqlite3_finalize(stmt_bal);
        }
        
        list.push_back(a);
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<Transaction> DBManager::getTransactions() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<Transaction> list;
    std::string sql = "SELECT id, date, description FROM transactions ORDER BY id DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return list;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Transaction t;
        t.id = sqlite3_column_int(stmt, 0);
        t.date = safe_col_text(stmt, 1);
        t.description = safe_col_text(stmt, 2);
        
        // Fetch splits
        std::string sql_sp = "SELECT s.id, s.account_id, s.amount, a.name FROM splits s JOIN accounts a ON s.account_id = a.id WHERE s.transaction_id = ?;";
        sqlite3_stmt* stmt_sp = nullptr;
        if (sqlite3_prepare_v2(m_db, sql_sp.c_str(), -1, &stmt_sp, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt_sp, 1, t.id);
            while (sqlite3_step(stmt_sp) == SQLITE_ROW) {
                Split s;
                s.id = sqlite3_column_int(stmt_sp, 0);
                s.transaction_id = t.id;
                s.account_id = sqlite3_column_int(stmt_sp, 1);
                s.amount = sqlite3_column_int64(stmt_sp, 2);
                s.account_name = safe_col_text(stmt_sp, 3);
                t.splits.push_back(s);
            }
            sqlite3_finalize(stmt_sp);
        }
        
        list.push_back(t);
    }
    sqlite3_finalize(stmt);
    return list;
}

int64_t DBManager::getAccountBalance(int account_id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "SELECT SUM(amount) FROM splits WHERE account_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int64_t balance = 0;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, account_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            balance = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return balance;
}

std::vector<InventoryItem> DBManager::getInventory() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<InventoryItem> list;
    std::string sql = "SELECT id, part_number, description, quantity_on_hand, reorder_point, wholesale_cost, retail_price FROM inventory ORDER BY part_number ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            InventoryItem item;
            item.id = sqlite3_column_int(stmt, 0);
            item.part_number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            item.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            item.quantity_on_hand = sqlite3_column_double(stmt, 3);
            item.reorder_point = sqlite3_column_double(stmt, 4);
            item.wholesale_cost = sqlite3_column_int64(stmt, 5);
            item.retail_price = sqlite3_column_int64(stmt, 6);
            list.push_back(item);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool DBManager::addInventoryItem(const InventoryItem& item) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "INSERT INTO inventory (part_number, description, quantity_on_hand, reorder_point, wholesale_cost, retail_price) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, item.part_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, item.quantity_on_hand);
    sqlite3_bind_double(stmt, 4, item.reorder_point);
    sqlite3_bind_int64(stmt, 5, item.wholesale_cost);
    sqlite3_bind_int64(stmt, 6, item.retail_price);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DBManager::updateInventoryItem(const InventoryItem& item) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "UPDATE inventory SET part_number = ?, description = ?, quantity_on_hand = ?, reorder_point = ?, wholesale_cost = ?, retail_price = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, item.part_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, item.quantity_on_hand);
    sqlite3_bind_double(stmt, 4, item.reorder_point);
    sqlite3_bind_int64(stmt, 5, item.wholesale_cost);
    sqlite3_bind_int64(stmt, 6, item.retail_price);
    sqlite3_bind_int(stmt, 7, item.id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<JobKit> DBManager::getJobKits() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<JobKit> list;
    std::string sql = "SELECT id, name, description FROM job_kits ORDER BY name ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            JobKit kit;
            kit.id = sqlite3_column_int(stmt, 0);
            kit.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            kit.description = desc ? desc : "";
            list.push_back(kit);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool DBManager::addJobKit(const JobKit& kit) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    std::string sql = "INSERT INTO job_kits (name, description) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_text(stmt, 1, kit.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, kit.description.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    int kit_id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    sqlite3_finalize(stmt);

    std::string sql_item = "INSERT INTO job_kit_items (kit_id, item_type, description, quantity, unit_price) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt_item = nullptr;
    if (sqlite3_prepare_v2(m_db, sql_item.c_str(), -1, &stmt_item, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& item : kit.items) {
        sqlite3_reset(stmt_item);
        sqlite3_bind_int(stmt_item, 1, kit_id);
        sqlite3_bind_text(stmt_item, 2, item.item_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt_item, 3, item.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt_item, 4, item.quantity);
        sqlite3_bind_int64(stmt_item, 5, item.unit_price);
        sqlite3_step(stmt_item);
    }
    sqlite3_finalize(stmt_item);

    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool DBManager::getJobKitDetails(int kit_id, JobKit& out_kit) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "SELECT id, name, description FROM job_kits WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool found = false;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, kit_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_kit.id = sqlite3_column_int(stmt, 0);
            out_kit.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            out_kit.description = desc ? desc : "";
            found = true;
        }
        sqlite3_finalize(stmt);
    }

    if (!found) return false;

    out_kit.items.clear();
    std::string sql_items = "SELECT id, item_type, description, quantity, unit_price FROM job_kit_items WHERE kit_id = ?;";
    sqlite3_stmt* stmt_items = nullptr;
    if (sqlite3_prepare_v2(m_db, sql_items.c_str(), -1, &stmt_items, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt_items, 1, kit_id);
        while (sqlite3_step(stmt_items) == SQLITE_ROW) {
            JobKitItem item;
            item.id = sqlite3_column_int(stmt_items, 0);
            item.kit_id = kit_id;
            item.item_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt_items, 1));
            item.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt_items, 2));
            item.quantity = sqlite3_column_double(stmt_items, 3);
            item.unit_price = sqlite3_column_int64(stmt_items, 4);
            out_kit.items.push_back(item);
        }
        sqlite3_finalize(stmt_items);
    }
    return true;
}

std::vector<Bay> DBManager::getBays() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<Bay> list;
    std::string sql = "SELECT id, name FROM bays ORDER BY name ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Bay b;
            b.id = sqlite3_column_int(stmt, 0);
            b.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            list.push_back(b);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::vector<BaySchedule> DBManager::getBaySchedules() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<BaySchedule> list;
    std::string sql = "SELECT s.id, s.bay_id, s.invoice_id, s.customer_name, s.vehicle_info, s.time_slot, s.notes, b.name "
                      "FROM bay_schedules s JOIN bays b ON s.bay_id = b.id ORDER BY s.time_slot ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BaySchedule s;
            s.id = sqlite3_column_int(stmt, 0);
            s.bay_id = sqlite3_column_int(stmt, 1);
            s.invoice_id = sqlite3_column_int(stmt, 2);
            s.customer_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            s.vehicle_info = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            s.time_slot = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            const char* notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            s.notes = notes ? notes : "";
            s.bay_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            list.push_back(s);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool DBManager::addBaySchedule(const BaySchedule& sched) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "INSERT INTO bay_schedules (bay_id, invoice_id, customer_name, vehicle_info, time_slot, notes) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, sched.bay_id);
    sqlite3_bind_int(stmt, 2, sched.invoice_id);
    sqlite3_bind_text(stmt, 3, sched.customer_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, sched.vehicle_info.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, sched.time_slot.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, sched.notes.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DBManager::deleteBaySchedule(int id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "DELETE FROM bay_schedules WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Invoice> DBManager::getVehicleServiceHistory(int vehicle_id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<Invoice> list;
    std::string sql = "SELECT id FROM invoices WHERE vehicle_id = ? ORDER BY id DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return list;

    sqlite3_bind_int(stmt, 1, vehicle_id);
    std::vector<int> ids;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // Since we now use std::recursive_mutex, we can safely call getInvoice directly.
    for (int id : ids) {
        Invoice inv;
        if (getInvoice(id, inv)) {
            list.push_back(inv);
        }
    }
    return list;
}

std::vector<Customer> DBManager::searchCustomers(const std::string& field, const std::string& value) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<Customer> list;
    
    std::string col = "last_name";
    if (field == "first_name") col = "first_name";
    else if (field == "phone_number") col = "phone_number";

    std::string sql = "SELECT id, last_name, first_name, middle_name, address, city, phone_number FROM customers WHERE " + col + " LIKE ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return list;

    std::string wild = value + "%";
    sqlite3_bind_text(stmt, 1, wild.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Customer c;
        c.id = sqlite3_column_int(stmt, 0);
        c.last_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        c.first_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* mid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        c.middle_name = mid ? mid : "";
        const char* addr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        c.address = addr ? addr : "";
        const char* city = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        c.city = city ? city : "";
        const char* phone = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        c.phone_number = phone ? phone : "";
        list.push_back(c);
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<RegisterRow> DBManager::getAccountRegister(int account_id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<RegisterRow> rows;
    
    std::string sql = 
        "SELECT t.date, t.description, s.amount "
        "FROM splits s "
        "JOIN transactions t ON s.transaction_id = t.id "
        "WHERE s.account_id = ? "
        "ORDER BY t.date ASC, t.id ASC;";
        
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return rows;
    
    sqlite3_bind_int(stmt, 1, account_id);
    
    int64_t running_balance = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RegisterRow r;
        r.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.amount = sqlite3_column_int64(stmt, 2);
        
        running_balance += r.amount;
        r.balance = running_balance;
        
        rows.push_back(r);
    }
    sqlite3_finalize(stmt);
    return rows;
}

bool DBManager::seedDefaultTemplates() {
    std::string sql = "SELECT COUNT(*) FROM print_templates;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            if (count > 0) return true;
        } else {
            sqlite3_finalize(stmt);
        }
    }

    PrintTemplate t1;
    t1.name = "Standard Tech Traveler";
    t1.content_html = R"html(<!DOCTYPE html><html><head><style>
body { font-family: 'Helvetica Neue', Arial, sans-serif; color: #333; margin: 20px; font-size: 13px; }
.header { border-bottom: 3px double #333; padding-bottom: 10px; margin-bottom: 20px; }
.title { font-size: 22px; font-weight: bold; text-transform: uppercase; letter-spacing: 1px; }
.barcode-box { float: right; text-align: right; }
.info-table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }
.info-table td { padding: 6px; border: 1px solid #ddd; vertical-align: top; width: 50%; }
.section-title { font-size: 14px; font-weight: bold; background-color: #f0f0f0; padding: 6px; margin-top: 20px; border-bottom: 2px solid #333; text-transform: uppercase; }
.checklist-table { width: 100%; margin-top: 10px; border-collapse: collapse; }
.checklist-table th, .checklist-table td { border: 1px solid #ccc; padding: 8px; text-align: left; }
.checklist-table th { background: #f7f7f7; }
</style></head><body>
<div class="header">
  <div class="barcode-box">
    {{BARCODE_SVG}}
  </div>
  <div class="title">{{SHOP_NAME}}</div>
  <div>Automotive Repair Tech Traveler Work Order</div>
  <div style="clear: both;"></div>
</div>
<table class="info-table">
  <tr>
    <td>
      <strong>CUSTOMER DETAILS:</strong><br/>
      Name: {{CUSTOMER_NAME}}<br/>
      Phone: {{CUSTOMER_PHONE}}<br/>
      Address: {{CUSTOMER_ADDRESS}}<br/>
    </td>
    <td>
      <strong>VEHICLE DETAILS:</strong><br/>
      License Plate: <strong>{{VEHICLE_PLATE}}</strong><br/>
      Year/Model: {{VEHICLE_YEAR_MODEL}}<br/>
      Engine Specs: {{VEHICLE_ENGINE}}<br/>
      VIN: {{VEHICLE_VIN}}<br/>
    </td>
  </tr>
</table>
<div class="section-title">Job Tasks & Parts Checklist</div>
{{TICKET_ITEMS_TABLE}}
<div class="section-title">Technician Notes & Quality Inspection Checklist</div>
<table class="checklist-table">
  <tr><th>Item</th><th>Status</th><th>Notes</th></tr>
  <tr><td>Fluid Levels & Leaks (Oil, Coolant, Brake, Transmission)</td><td>[ ] OK  [ ] LOW  [ ] LEAK</td><td>________________________________________</td></tr>
  <tr><td>Brakes Inspection (Front Pads: ____mm, Rear Pads: ____mm)</td><td>[ ] OK  [ ] WEAR  [ ] REQ</td><td>________________________________________</td></tr>
  <tr><td>Tire Pressures & Condition</td><td>[ ] OK  [ ] ROTATE  [ ] REQ</td><td>________________________________________</td></tr>
</table>
</body></html>)html";
    t1.is_active = 1;
    addPrintTemplateInternal(t1);

    PrintTemplate t2;
    t2.name = "Compact Thermal Receipt";
    t2.content_html = R"html(<!DOCTYPE html><html><head><style>
body { font-family: 'Courier New', monospace; width: 280px; margin: 0; padding: 5px; font-size: 11px; color: black; }
.center { text-align: center; }
.bold { font-weight: bold; }
.divider { border-top: 1px dashed black; margin: 6px 0; }
table { width: 100%; border-collapse: collapse; }
td { vertical-align: top; font-size: 11px; }
.right { text-align: right; }
</style></head><body>
<div class="center bold">{{SHOP_NAME}}</div>
<div class="center">Automotive Repair Receipt</div>
<div class="divider"></div>
<div>TICKET #: {{TICKET_ID}}</div>
<div>DATE  : {{DATE_CREATED}}</div>
<div>PLATE : {{VEHICLE_PLATE}}</div>
<div class="divider"></div>
<div class="bold">ITEMS:</div>
{{TICKET_ITEMS_TABLE}}
<div class="divider"></div>
<table style="width:100%;">
  <tr><td>SUBTOTAL</td><td class="right">{{TICKET_SUBTOTAL}}</td></tr>
  <tr><td>SUPPLIES</td><td class="right">{{TICKET_SUPPLIES}}</td></tr>
  <tr><td>TAX</td><td class="right">{{TICKET_TAX}}</td></tr>
  <tr class="bold"><td>GRAND TOTAL</td><td class="right">{{TICKET_GRAND_TOTAL}}</td></tr>
</table>
<div class="divider"></div>
<div class="center bold">THANK YOU!</div>
</body></html>)html";
    t2.is_active = 0;
    addPrintTemplateInternal(t2);

    PrintTemplate t3;
    t3.name = "Formal Customer Invoice";
    t3.content_html = R"html(<!DOCTYPE html><html><head><style>
body { font-family: 'Helvetica Neue', Arial, sans-serif; color: #333; margin: 30px; font-size: 13px; }
.invoice-title { font-size: 28px; font-weight: bold; color: #1e88e5; }
.divider { border-bottom: 2px solid #1e88e5; margin: 15px 0; }
.info-table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }
.info-table td { padding: 6px; border: none; vertical-align: top; width: 50%; }
.totals-table { width: 40%; margin-left: 60%; margin-top: 20px; }
.totals-table td { padding: 6px; border-bottom: 1px solid #ddd; }
.bold { font-weight: bold; }
.right { text-align: right; }
</style></head><body>
<table class="info-table">
  <tr>
    <td>
      <span class="invoice-title">{{SHOP_NAME}}</span><br/>
      Sovereign Auto Repair Specialists
    </td>
    <td class="right">
      <span style="font-size: 20px; font-weight: bold; color: #555;">INVOICE</span><br/>
      Invoice ID: #{{TICKET_ID}}<br/>
      Date: {{DATE_CREATED}}
    </td>
  </tr>
</table>
<div class="divider"></div>
<table class="info-table">
  <tr>
    <td>
      <strong>BILL TO:</strong><br/>
      Name: {{CUSTOMER_NAME}}<br/>
      Phone: {{CUSTOMER_PHONE}}<br/>
      Address: {{CUSTOMER_ADDRESS}}<br/>
    </td>
    <td>
      <strong>VEHICLE DETAILS:</strong><br/>
      License Plate: <strong>{{VEHICLE_PLATE}}</strong><br/>
      Year/Model: {{VEHICLE_YEAR_MODEL}}<br/>
      Engine Specs: {{VEHICLE_ENGINE}}
    </td>
  </tr>
</table>
<div class="bold" style="margin-top:20px; margin-bottom:10px;">SERVICES RENDERED:</div>
{{TICKET_ITEMS_TABLE}}
<table class="totals-table">
  <tr><td>Subtotal</td><td class="right">{{TICKET_SUBTOTAL}}</td></tr>
  <tr><td>Shop Supplies</td><td class="right">{{TICKET_SUPPLIES}}</td></tr>
  <tr><td>Sales Tax</td><td class="right">{{TICKET_TAX}}</td></tr>
  <tr class="bold" style="background:#f0f4f8;"><td>Grand Total</td><td class="right">{{TICKET_GRAND_TOTAL}}</td></tr>
</table>
</body></html>)html";
    t3.is_active = 0;
    addPrintTemplateInternal(t3);

    return true;
}

std::vector<PrintTemplate> DBManager::getPrintTemplates() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<PrintTemplate> list;
    std::string sql = "SELECT id, name, content_html, is_active FROM print_templates ORDER BY id ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return list;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PrintTemplate t;
        t.id = sqlite3_column_int(stmt, 0);
        t.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        t.content_html = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        t.is_active = sqlite3_column_int(stmt, 3);
        list.push_back(t);
    }
    sqlite3_finalize(stmt);
    return list;
}

bool DBManager::addPrintTemplateInternal(const PrintTemplate& tmpl) {
    std::string sql = "INSERT INTO print_templates (name, content_html, is_active) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, tmpl.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, tmpl.content_html.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, tmpl.is_active);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DBManager::addPrintTemplate(const PrintTemplate& tmpl) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return addPrintTemplateInternal(tmpl);
}

bool DBManager::updatePrintTemplate(const PrintTemplate& tmpl) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "UPDATE print_templates SET name = ?, content_html = ?, is_active = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, tmpl.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, tmpl.content_html.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, tmpl.is_active);
    sqlite3_bind_int(stmt, 4, tmpl.id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DBManager::deletePrintTemplate(int id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "DELETE FROM print_templates WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DBManager::setActivePrintTemplate(int id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql_deact = "UPDATE print_templates SET is_active = 0;";
    // Simple helper sql execution
    sqlite3_stmt* st_d = nullptr;
    if (sqlite3_prepare_v2(m_db, sql_deact.c_str(), -1, &st_d, nullptr) == SQLITE_OK) {
        sqlite3_step(st_d);
        sqlite3_finalize(st_d);
    }

    std::string sql_act = "UPDATE print_templates SET is_active = 1 WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql_act.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

PrintTemplate DBManager::getActivePrintTemplate() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    PrintTemplate t;
    std::string sql = "SELECT id, name, content_html, is_active FROM print_templates WHERE is_active = 1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            t.id = sqlite3_column_int(stmt, 0);
            t.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            t.content_html = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            t.is_active = sqlite3_column_int(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }
    return t;
}

std::string DBManager::getSetting(const std::string& key, const std::string& default_value) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    std::string val = default_value;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* res = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (res) val = res;
        }
        sqlite3_finalize(stmt);
    }
    return val;
}

bool DBManager::setSetting(const std::string& key, const std::string& value) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<std::string> DBManager::getCarMakes() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<std::string> list;
    // Check if table exists first by querying sqlite_master
    std::string check_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='car_catalog';";
    sqlite3_stmt* check_stmt = nullptr;
    bool exists = false;
    if (sqlite3_prepare_v2(m_db, check_sql.c_str(), -1, &check_stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            exists = true;
        }
        sqlite3_finalize(check_stmt);
    }
    if (!exists) return list;

    std::string sql = "SELECT DISTINCT make FROM car_catalog WHERE make != '' ORDER BY make ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (val) list.push_back(val);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::vector<std::string> DBManager::getCarModels(const std::string& make) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<std::string> list;
    std::string sql = "SELECT DISTINCT model FROM car_catalog WHERE make = ? AND model != '' ORDER BY model ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, make.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (val) list.push_back(val);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::string DBManager::getCarEngineSpecs(const std::string& make, const std::string& model) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string specs = "";
    std::string sql = "SELECT engine_size, cylinders, horsepower, fuel_type FROM car_catalog WHERE make = ? AND model = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, make.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, model.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* size = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* cyl = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* hp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* fuel = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            std::vector<std::string> parts;
            if (size && strlen(size) > 0) parts.push_back(std::string(size) + "L");
            if (cyl && strlen(cyl) > 0) parts.push_back(std::string(cyl) + "cyl");
            if (hp && strlen(hp) > 0) parts.push_back(std::string(hp) + "HP");
            if (fuel && strlen(fuel) > 0) parts.push_back(fuel);
            
            for (size_t i = 0; i < parts.size(); ++i) {
                specs += parts[i];
                if (i + 1 < parts.size()) specs += ", ";
            }
        }
        sqlite3_finalize(stmt);
    }
    return specs;
}

} // namespace tuxrepair
