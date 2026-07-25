#include <QApplication>
#include <QDir>
#include <QFile>
#include "main_window.h"
#include "db_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QString dbPath = QDir::current().filePath("tuxrepair.db");

    // Remove old database file for a clean reset
    QFile::remove(dbPath);
    
    auto db = std::make_shared<tuxrepair::DBManager>();
    if (!db->open(dbPath.toStdString())) {
        std::cerr << "Failed to open SQLite database at: " << dbPath.toStdString() << std::endl;
        return 1;
    }

    // =========================================================================
    // SEED RICH PREPOPULATED SHOP DATA FOR COMPREHENSIVE TESTING
    // =========================================================================
    
    // 1. Prepopulate Inventory Catalog
    std::vector<tuxrepair::InventoryItem> seed_inventory;
    {
        tuxrepair::InventoryItem item;
        item.part_number = "OIL-5W30-SYN"; item.description = "Full Synthetic Motor Oil 5W-30 (Qt)";
        item.quantity_on_hand = 120.0; item.reorder_point = 10.0; item.wholesale_cost = 450; item.retail_price = 999;
        seed_inventory.push_back(item);
    }
    {
        tuxrepair::InventoryItem item;
        item.part_number = "OIL-FLTR-OEM"; item.description = "OEM Oil Filter High-Efficiency Element";
        item.quantity_on_hand = 45.0; item.reorder_point = 5.0; item.wholesale_cost = 350; item.retail_price = 1299;
        seed_inventory.push_back(item);
    }
    {
        tuxrepair::InventoryItem item;
        item.part_number = "BRK-PAD-CER"; item.description = "Ceramic Front Brake Pad Set - Premium";
        item.quantity_on_hand = 15.0; item.reorder_point = 3.0; item.wholesale_cost = 3200; item.retail_price = 6999;
        seed_inventory.push_back(item);
    }
    {
        tuxrepair::InventoryItem item;
        item.part_number = "BRK-RTR-VENT"; item.description = "Vented Front Brake Rotor Assembly";
        item.quantity_on_hand = 12.0; item.reorder_point = 2.0; item.wholesale_cost = 4500; item.retail_price = 8999;
        seed_inventory.push_back(item);
    }
    {
        tuxrepair::InventoryItem item;
        item.part_number = "FLUID-DOT4"; item.description = "DOT4 Synthetic Brake Fluid 32oz";
        item.quantity_on_hand = 25.0; item.reorder_point = 5.0; item.wholesale_cost = 600; item.retail_price = 1499;
        seed_inventory.push_back(item);
    }
    for (const auto& item : seed_inventory) {
        db->addInventoryItem(item);
    }

    // 2. Customer 1: Robert Miller - 2021 Ford F-150
    tuxrepair::Customer c1;
    c1.first_name = "Robert"; c1.last_name = "Miller"; c1.middle_name = "J.";
    c1.address = "742 Evergreen Terrace"; c1.city = "Springfield, OR";
    c1.phone_number = "(541) 555-0147"; c1.email = "rmiller@example.com";
    int c1_id = db->insertCustomer(c1);

    int v1_id = -1;
    if (c1_id != -1) {
        tuxrepair::Vehicle v1;
        v1.customer_id = c1_id; v1.license_plate = "7ABC123"; v1.vin = "1FTFW1E84MF123456";
        v1.year = 2021; v1.model = "Ford F-150 Lariat 4WD"; v1.engine_specs = "3.5L EcoBoost V6 Turbo";
        v1_id = db->insertVehicle(v1);
    }

    // 3. Customer 2: Sarah Jenkins - 2019 Toyota Camry
    tuxrepair::Customer c2;
    c2.first_name = "Sarah"; c2.last_name = "Jenkins"; c2.middle_name = "M.";
    c2.address = "1204 Pine Street"; c2.city = "Eugene, OR";
    c2.phone_number = "(541) 555-0892"; c2.email = "sjenkins@example.com";
    int c2_id = db->insertCustomer(c2);

    int v2_id = -1;
    if (c2_id != -1) {
        tuxrepair::Vehicle v2;
        v2.customer_id = c2_id; v2.license_plate = "8XYZ456"; v2.vin = "4T1B11HK5KU789012";
        v2.year = 2019; v2.model = "Toyota Camry SE"; v2.engine_specs = "2.5L 4-Cyl DOHC 16V";
        v2_id = db->insertVehicle(v2);
    }

    // 4. Customer 3: Marcus Vance - 2018 BMW 330i
    tuxrepair::Customer c3;
    c3.first_name = "Marcus"; c3.last_name = "Vance"; c3.middle_name = "E.";
    c3.address = "450 Oakridge Blvd"; c3.city = "Portland, OR";
    c3.phone_number = "(503) 555-0311"; c3.email = "mvance@example.com";
    int c3_id = db->insertCustomer(c3);

    int v3_id = -1;
    if (c3_id != -1) {
        tuxrepair::Vehicle v3;
        v3.customer_id = c3_id; v3.license_plate = "9LMN789"; v3.vin = "WBA8B9G59JNW34567";
        v3.year = 2018; v3.model = "BMW 330i xDrive"; v3.engine_specs = "2.0L Turbo Inline-4";
        v3_id = db->insertVehicle(v3);
    }

    // 5. Prepopulate Work Orders with Line Items & Notes
    if (c1_id != -1 && v1_id != -1) {
        int inv1 = db->createInvoice(c1_id, v1_id, "Estimate", 45210, "2026-07-24");
        db->updateInvoiceHeader(inv1, "Estimate", 45210, 45215, "In Progress", false, "Bob (Tech)");
        db->updateInvoiceNotes(inv1, "Customer reported squeaking noise when braking above 40 mph.",
                               "Full front brake pad & rotor replacement estimate provided.",
                               "Front pads worn to 2mm. Rotors resurfacing limit exceeded; replacing both.",
                               "2021 F-150 4WD - EcoBoost 3.5L V6", "Customer verbal authorization given over phone.");

        std::vector<tuxrepair::InvoiceItem> items1;
        tuxrepair::InvoiceItem i1_1;
        i1_1.invoice_id = inv1; i1_1.part_number = "BRK-PAD-CER"; i1_1.description = "Ceramic Front Brake Pad Set";
        i1_1.quantity = 1.0; i1_1.unit_price = 6999; i1_1.item_type = "Part"; i1_1.tech_assigned = "Bob (Tech)";
        items1.push_back(i1_1);

        tuxrepair::InvoiceItem i1_2;
        i1_2.invoice_id = inv1; i1_2.part_number = "BRK-RTR-VENT"; i1_2.description = "Vented Front Brake Rotors (Pair)";
        i1_2.quantity = 2.0; i1_2.unit_price = 8999; i1_2.item_type = "Part"; i1_2.tech_assigned = "Bob (Tech)";
        items1.push_back(i1_2);

        tuxrepair::InvoiceItem i1_3;
        i1_3.invoice_id = inv1; i1_3.part_number = "LABOR-BRK"; i1_3.description = "Front Brake System Overhaul & Service";
        i1_3.quantity = 2.0; i1_3.unit_price = 12500; i1_3.item_type = "Labor"; i1_3.tech_assigned = "Bob (Tech)";
        items1.push_back(i1_3);

        db->saveInvoiceItems(inv1, items1);
    }

    if (c2_id != -1 && v2_id != -1) {
        int inv2 = db->createInvoice(c2_id, v2_id, "Quote", 32100, "2026-07-25");
        db->updateInvoiceHeader(inv2, "Quote", 32100, 32102, "New", false, "Jane (Tech)");
        db->updateInvoiceNotes(inv2, "Regular 30k maintenance package quote requested.",
                               "Synthetic oil change & carbon filter replacement.",
                               "All fluids inspected and topped off. Tire tread 7/32 all around.",
                               "2019 Camry SE - 2.5L 4-Cyl", "Awaiting customer approval.");

        std::vector<tuxrepair::InvoiceItem> items2;
        tuxrepair::InvoiceItem i2_1;
        i2_1.invoice_id = inv2; i2_1.part_number = "OIL-5W30-SYN"; i2_1.description = "Full Synthetic Oil 5W-30";
        i2_1.quantity = 5.0; i2_1.unit_price = 999; i2_1.item_type = "Part"; i2_1.tech_assigned = "Jane (Tech)";
        items2.push_back(i2_1);

        tuxrepair::InvoiceItem i2_2;
        i2_2.invoice_id = inv2; i2_2.part_number = "OIL-FLTR-OEM"; i2_2.description = "OEM Toyota Oil Filter Element";
        i2_2.quantity = 1.0; i2_2.unit_price = 1299; i2_2.item_type = "Part"; i2_2.tech_assigned = "Jane (Tech)";
        items2.push_back(i2_2);

        tuxrepair::InvoiceItem i2_3;
        i2_3.invoice_id = inv2; i2_3.part_number = "LABOR-OIL"; i2_3.description = "Full Synthetic Oil & Filter Service";
        i2_3.quantity = 0.5; i2_3.unit_price = 12500; i2_3.item_type = "Labor"; i2_3.tech_assigned = "Jane (Tech)";
        items2.push_back(i2_3);

        db->saveInvoiceItems(inv2, items2);
    }

    // 6. Add 10 Additional Realistic Customers & Vehicles for Comprehensive Testing
    struct SampleSeed {
        std::string first; std::string last; std::string phone; std::string email;
        std::string plate; std::string vin; int year; std::string model; std::string engine;
        std::string status; std::string ticket_type;
    };

    std::vector<SampleSeed> extra_seeds = {
        {"David", "Harrison", "(541) 555-0912", "dharrison@example.com", "6TRK891", "1GNSKBE30DR112233", 2020, "Chevrolet Tahoe LT", "5.3L V8 EcoTec3", "Approved", "Invoice"},
        {"Emily", "Watson", "(503) 555-0422", "ewatson@example.com", "5ABC432", "5NPD84LF2KH445566", 2017, "Hyundai Sonata SE", "2.4L 4-Cyl DOHC", "In Progress", "Estimate"},
        {"James", "O'Connor", "(541) 555-0733", "joconnor@example.com", "8DEF654", "3FA6P0H78HR778899", 2016, "Ford Fusion Titanium", "2.0L EcoBoost Turbo", "Ready", "Invoice"},
        {"Jessica", "Taylor", "(503) 555-0188", "jtaylor@example.com", "7GHI987", "WAUZZZ8K9BA101112", 2022, "Audi A4 Quattro", "2.0L TFSI Turbo", "Waiting on Parts", "Estimate"},
        {"Carlos", "Rodriguez", "(541) 555-0644", "crodriguez@example.com", "4JKL321", "1HGCR2F83HA131415", 2017, "Honda Accord EX-L", "3.5L V6 i-VTEC", "New", "Quote"},
        {"Amanda", "Chen", "(503) 555-0555", "achen@example.com", "9MNO654", "JN1AZ4EH8DM161718", 2019, "Nissan Rogue SL", "2.5L 4-Cyl DOHC", "Approved", "Estimate"},
        {"Brian", "Kowalski", "(541) 555-0277", "bkowalski@example.com", "3PQR987", "4T1B11HK5LU192021", 2020, "Toyota RAV4 XLE", "2.5L 4-Cyl Dynamic Force", "Closed", "Invoice"},
        {"Megan", "Brooks", "(503) 555-0999", "mbrooks@example.com", "6STU123", "1FMCU9GD5KUB22232", 2018, "Ford Escape SEL", "1.5L EcoBoost 3-Cyl", "Awaiting Approval", "Estimate"},
        {"Kevin", "Patel", "(541) 555-0811", "kpatel@example.com", "2VWX456", "3VW2B7AJ9HM242526", 2019, "Volkswagen Jetta R-Line", "1.4L TSI Turbo", "New", "Quote"},
        {"Rachel", "Foster", "(503) 555-0344", "rfoster@example.com", "8YZA789", "5YJSA1E28HF272829", 2021, "Tesla Model S Dual Motor", "Electric AWD", "In Progress", "Estimate"}
    };

    for (const auto& s : extra_seeds) {
        tuxrepair::Customer c;
        c.first_name = s.first; c.last_name = s.last;
        c.phone_number = s.phone; c.email = s.email;
        int cid = db->insertCustomer(c);
        if (cid != -1) {
            tuxrepair::Vehicle v;
            v.customer_id = cid; v.license_plate = s.plate; v.vin = s.vin;
            v.year = s.year; v.model = s.model; v.engine_specs = s.engine;
            int vid = db->insertVehicle(v);
            if (vid != -1) {
                int inv = db->createInvoice(cid, vid, s.ticket_type, 25000 + (cid * 1230), "2026-07-25");
                db->updateInvoiceHeader(inv, s.ticket_type, 25000 + (cid * 1230), 25005 + (cid * 1230), s.status, false, "Office");
                
                // Add sample line item
                std::vector<tuxrepair::InvoiceItem> items;
                tuxrepair::InvoiceItem item1;
                item1.invoice_id = inv; item1.part_number = "SPARK-IRID"; item1.description = "Iridium Spark Plugs & Multi-Point Inspection";
                item1.quantity = 4.0; item1.unit_price = 1599; item1.item_type = "Part"; item1.tech_assigned = "Bob (Tech)";
                items.push_back(item1);

                tuxrepair::InvoiceItem item2;
                item2.invoice_id = inv; item2.part_number = "LABOR-DIAG"; item2.description = "Comprehensive Diagnostic & Tune-up Labor";
                item2.quantity = 1.5; item2.unit_price = 12500; item2.item_type = "Labor"; item2.tech_assigned = "Bob (Tech)";
                items.push_back(item2);

                db->saveInvoiceItems(inv, items);
            }
        }
    }

    tuxrepair::MainWindow w(db);
    w.show();

    return app.exec();
}
