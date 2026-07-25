#ifndef TUXREPAIR_MODELS_H
#define TUXREPAIR_MODELS_H

#include <string>
#include <vector>
#include <cstdint>

namespace tuxrepair {

struct Customer {
    int id = 0;
    std::string last_name;
    std::string first_name;
    std::string middle_name;
    std::string address;
    std::string city;
    std::string phone_number;
    std::string email;
    std::string preferred_contact;
    std::string notes;
};

struct Vehicle {
    int id = 0;
    int customer_id = 0;
    std::string license_plate;
    std::string vin;
    int year = 0;
    std::string model;
    std::string engine_specs;
    std::string trim;
    std::string transmission;
    std::string color;
};

struct InvoiceItem {
    int id = 0;
    int invoice_id = 0;
    std::string part_number; // Part SKU or Labor Code
    std::string description;
    double quantity = 0.0;
    int64_t unit_price = 0; // in cents
    std::string specification; // raw data or tech details
    std::string item_type = "Part"; // "Part", "Labor", "Fee", "Sublet", "Discount"
    std::string tech_assigned;
    double discount_percent = 0.0;
    bool taxable = true;
    std::string line_notes;
    std::string status = "Approved"; // "Pending", "Approved", "Declined", "Ordered", "Installed"
};

struct Invoice {
    int id = 0;
    int customer_id = 0;
    int vehicle_id = 0;
    std::string ticket_type; // "Quote", "Estimate", "Invoice"
    int mileage_in = 0;
    int mileage_out = 0;
    std::string date_created;
    std::string status; // "New", "Intake", "Estimate", "Awaiting Approval", "Approved", "In Progress", "Waiting on Parts", "Ready", "Invoiced", "Closed"
    bool supplies_removed = false;
    std::string writer; // secretary / writer who did the billing
    std::string internal_notes;
    std::string customer_notes;
    std::string tech_notes;
    std::string vehicle_notes;
    std::string auth_notes;
    int64_t prepayment_cents = 0;
    std::string signature_data; // Base64 encoded or string representation
    int64_t posted_tx_id = 0;   // 0 = not posted to ledger; non-0 = transactions.id of the posting

    std::vector<InvoiceItem> items;
    Customer customer;
    Vehicle vehicle;
};

struct StatusHistoryEntry {
    int id = 0;
    int invoice_id = 0;
    std::string status;
    std::string timestamp;
    std::string user_name;
};

struct Attachment {
    int id = 0;
    int invoice_id = 0;
    std::string file_path;
    std::string file_name;
    std::string upload_time;
    bool is_internal = true;
    int line_item_id = -1;
};

struct Account {
    int id = 0;
    std::string name;
    std::string type; // "Asset", "Liability", "Equity", "Income", "Expense"
    int64_t balance = 0; // calculated dynamically
};

struct Split {
    int id = 0;
    int transaction_id = 0;
    int account_id = 0;
    int64_t amount = 0; // in cents. Positive = Debit, Negative = Credit
    std::string account_name; // filled on query for convenience
};

struct Transaction {
    int id = 0;
    std::string date;
    std::string description;
    std::vector<Split> splits;
};

struct RegisterRow {
    std::string date;
    std::string description;
    int64_t amount = 0;
    int64_t balance = 0;
};

struct InventoryItem {
    int id = 0;
    std::string part_number;
    std::string description;
    double quantity_on_hand = 0.0;
    double reorder_point = 5.0;
    int64_t wholesale_cost = 0; // cents
    int64_t retail_price = 0; // cents
};

struct JobKitItem {
    int id = 0;
    int kit_id = 0;
    std::string item_type; // "Part" or "Labor"
    std::string description;
    double quantity = 1.0;
    int64_t unit_price = 0; // cents
};

struct JobKit {
    int id = 0;
    std::string name;
    std::string description;
    std::vector<JobKitItem> items;
};

struct Bay {
    int id = 0;
    std::string name;
};

struct BaySchedule {
    int id = 0;
    int bay_id = 0;
    int invoice_id = 0;
    std::string customer_name;
    std::string vehicle_info;
    std::string time_slot; // "YYYY-MM-DD HH:MM"
    std::string notes;
    std::string bay_name; // convenience
};

struct PrintTemplate {
    int id = 0;
    std::string name;
    std::string content_html;
    int is_active = 0;
};

} // namespace tuxrepair

#endif // TUXREPAIR_MODELS_H
