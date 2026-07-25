// Basic round-trip tests for DBManager. These verify the happy path of the
// core persistence layer against an in-memory SQLite database — fast, headless,
// no filesystem footprint, no Qt.

#include "test_runner.h"
#include "db_manager.h"

#include <memory>

using namespace tuxrepair;

// Each test case opens its own fresh in-memory DB so cases can't bleed state.
// CHECK_TRUE (not ASSERT_TRUE) because this is a helper, not a test case —
// ASSERT_* would emit `return false;` which can't convert to shared_ptr.
static std::shared_ptr<DBManager> fresh_db() {
    auto db = std::make_shared<DBManager>();
    if (!CHECK_TRUE(db->open(":memory:"))) return nullptr;
    return db;
}

// Helper: insert a throwaway customer + vehicle, return the vehicle_id.
// Returns -1 (and records a failure) on any error.
static int seed_customer_vehicle(DBManager& db) {
    Customer c;
    c.first_name = "Jane";
    c.last_name = "Driver";
    c.phone_number = "555-0001";
    int cid = db.insertCustomer(c);
    if (!CHECK_TRUE(cid >= 1)) return -1;

    Vehicle v;
    v.customer_id = cid;
    v.license_plate = "TEST001";
    v.vin = "1HGBH41JXMN109999";
    v.year = 2020;
    v.model = "Honda Civic";
    int vid = db.insertVehicle(v);
    if (!CHECK_TRUE(vid >= 1)) return -1;
    return vid;
}

bool test_open_in_memory() {
    auto db = std::make_shared<DBManager>();
    ASSERT_TRUE(db->open(":memory:"));
    // Schema seeding happens inside open(): default accounts must exist.
    auto accounts = db->getAccounts();
    ASSERT_FALSE(accounts.empty());
    return true;
}

bool test_open_failure_returns_false() {
    // Opening a path whose parent directory doesn't exist must fail.
    auto db = std::make_shared<DBManager>();
    ASSERT_FALSE(db->open("/nonexistent/path/that/cannot/be/created.db"));
    return true;
}

bool test_seed_accounts_present() {
    auto db = fresh_db();
    auto accounts = db->getAccounts();
    // Every account the finalize/payment flow depends on must exist after
    // schema init. Phase 2 added Accounts Receivable and Customer Deposits.
    bool has_checking = false, has_parts_inc = false, has_labor_inc = false;
    bool has_cogs = false, has_tax = false, has_parts_inv = false;
    bool has_ar = false, has_deposits = false;
    for (const auto& a : accounts) {
        if (a.name == "Checking Asset")             has_checking  = true;
        if (a.name == "Parts Income")               has_parts_inc = true;
        if (a.name == "Labor Income")               has_labor_inc = true;
        if (a.name == "Cost of Goods Sold Expense") has_cogs      = true;
        if (a.name == "Sales Tax Liability")        has_tax       = true;
        if (a.name == "Parts Inventory Asset")      has_parts_inv = true;
        if (a.name == "Accounts Receivable")        has_ar        = true;
        if (a.name == "Customer Deposits")          has_deposits  = true;
    }
    ASSERT_TRUE(has_checking);
    ASSERT_TRUE(has_parts_inc);
    ASSERT_TRUE(has_labor_inc);
    ASSERT_TRUE(has_cogs);
    ASSERT_TRUE(has_tax);
    ASSERT_TRUE(has_parts_inv);
    ASSERT_TRUE(has_ar);
    ASSERT_TRUE(has_deposits);
    return true;
}

bool test_customer_vehicle_roundtrip() {
    auto db = fresh_db();
    Customer c;
    c.first_name = "Maria";
    c.last_name = "Garcia";
    c.middle_name = "E";
    c.address = "100 Oak Ave";
    c.city = "Tulare, CA";
    c.phone_number = "555-0199";
    c.email = "maria@example.com";
    int cid = db->insertCustomer(c);
    ASSERT_GE(cid, 1);

    Customer fetched;
    ASSERT_TRUE(db->getCustomer(cid, fetched));
    ASSERT_EQ(fetched.id, cid);
    ASSERT_TRUE(fetched.first_name == "Maria");
    ASSERT_TRUE(fetched.last_name == "Garcia");
    ASSERT_TRUE(fetched.email == "maria@example.com");

    // Update
    fetched.last_name = "Lopez";
    ASSERT_TRUE(db->updateCustomer(fetched));
    Customer fetched2;
    ASSERT_TRUE(db->getCustomer(cid, fetched2));
    ASSERT_TRUE(fetched2.last_name == "Lopez");
    return true;
}

bool test_vehicle_search_by_plate() {
    auto db = fresh_db();
    seed_customer_vehicle(*db);  // plate TEST001
    // searchVehiclesByPlate uses a "plate%" LIKE prefix.
    auto results = db->searchVehiclesByPlate("TEST");
    ASSERT_FALSE(results.empty());
    ASSERT_TRUE(results[0].license_plate == "TEST001");
    return true;
}

bool test_invoice_create_and_fetch() {
    auto db = fresh_db();
    int vid = seed_customer_vehicle(*db);

    int inv_id = db->createInvoice(1, vid, "Estimate", 12345, "2026-07-19");
    ASSERT_GE(inv_id, 1);

    Invoice inv;
    ASSERT_TRUE(db->getInvoice(inv_id, inv));
    ASSERT_EQ(inv.id, inv_id);
    ASSERT_TRUE(inv.ticket_type == "Estimate");
    ASSERT_EQ(inv.mileage_in, 12345);
    ASSERT_TRUE(inv.items.empty());  // no items yet
    return true;
}

bool test_save_and_load_invoice_items() {
    auto db = fresh_db();
    int vid = seed_customer_vehicle(*db);
    int inv_id = db->createInvoice(1, vid, "Invoice", 100, "2026-07-19");

    std::vector<InvoiceItem> items;
    InvoiceItem p;
    p.part_number = "5W30-SB";
    p.description = "Synthetic Oil Quart";
    p.quantity = 5.0;
    p.unit_price = 500;         // $5.00 = 500 cents
    p.specification = "Part";   // NOTE: classification currently keys off this field
    p.item_type = "Part";
    items.push_back(p);

    InvoiceItem l;
    l.part_number = "LBR-OIL";
    l.description = "Labor - Oil Change";
    l.quantity = 1.0;
    l.unit_price = 2500;        // $25.00
    l.specification = "Labor";
    l.item_type = "Labor";
    items.push_back(l);

    ASSERT_TRUE(db->saveInvoiceItems(inv_id, items));

    Invoice inv;
    ASSERT_TRUE(db->getInvoice(inv_id, inv));
    ASSERT_EQ(static_cast<long long>(inv.items.size()), 2LL);
    ASSERT_TRUE(inv.items[0].description == "Synthetic Oil Quart");
    ASSERT_EQ(inv.items[0].unit_price, 500);
    ASSERT_EQ(inv.items[1].unit_price, 2500);
    return true;
}

bool test_settings_roundtrip() {
    auto db = fresh_db();
    // Default when unset
    ASSERT_TRUE(db->getSetting("missing_key", "default123") == "default123");
    // Set and get
    ASSERT_TRUE(db->setSetting("tax_rate", "0.0825"));
    ASSERT_TRUE(db->getSetting("tax_rate") == "0.0825");
    return true;
}

RUN_TESTS {
    RUN_TEST(test_open_in_memory);
    RUN_TEST(test_open_failure_returns_false);
    RUN_TEST(test_seed_accounts_present);
    RUN_TEST(test_customer_vehicle_roundtrip);
    RUN_TEST(test_vehicle_search_by_plate);
    RUN_TEST(test_invoice_create_and_fetch);
    RUN_TEST(test_save_and_load_invoice_items);
    RUN_TEST(test_settings_roundtrip);
    TEST_RETURN;
}
