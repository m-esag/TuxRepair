#ifndef TUXREPAIR_DB_MANAGER_H
#define TUXREPAIR_DB_MANAGER_H

#include "models.h"
#include <sqlite3.h>
#include <string>
#include <vector>
#include <mutex>
#include <map>

namespace tuxrepair {

class DBManager {
public:
    DBManager();
    ~DBManager();

    bool open(const std::string& db_path);
    void close();

    // Customer operations
    int insertCustomer(const Customer& customer);
    bool getCustomer(int id, Customer& out_customer);
    bool updateCustomer(const Customer& customer);

    // Vehicle operations
    int insertVehicle(const Vehicle& vehicle);
    bool getVehicle(int id, Vehicle& out_vehicle);
    std::vector<Vehicle> searchVehiclesByPlate(const std::string& plate);
    bool updateVehicle(const Vehicle& vehicle);

    // Invoice operations
    int createInvoice(int customer_id, int vehicle_id, const std::string& ticket_type, int mileage_in, const std::string& date_created);
    bool getInvoice(int id, Invoice& out_invoice);
    std::vector<Invoice> getAllInvoices();
    bool updateInvoiceHeader(int id, const std::string& ticket_type, int mileage_in, int mileage_out, const std::string& status, bool supplies_removed = false, const std::string& writer = "Office");
    bool updateInvoiceVehicle(int invoice_id, int vehicle_id);
    bool saveInvoiceItems(int invoice_id, const std::vector<InvoiceItem>& items);
    bool finalizeInvoice(int invoice_id, int64_t parts_cost_cents, double tax_rate = 0.08, bool supplies_removed = false);

    // Accounting operations
    std::vector<Account> getAccounts();
    std::vector<Transaction> getTransactions();
    int64_t getAccountBalance(int account_id);

    // Inventory operations
    std::vector<InventoryItem> getInventory();
    bool addInventoryItem(const InventoryItem& item);
    bool updateInventoryItem(const InventoryItem& item);
    
    // Job Kit operations
    std::vector<JobKit> getJobKits();
    bool addJobKit(const JobKit& kit);
    bool getJobKitDetails(int kit_id, JobKit& out_kit);

    // Scheduling operations
    std::vector<Bay> getBays();
    std::vector<BaySchedule> getBaySchedules();
    bool addBaySchedule(const BaySchedule& sched);
    bool deleteBaySchedule(int id);

    // Car Catalog operations
    std::vector<std::string> getCarMakes();
    std::vector<std::string> getCarModels(const std::string& make);
    std::string getCarEngineSpecs(const std::string& make, const std::string& model);

    // Service History and Advanced Search
    std::vector<Invoice> getVehicleServiceHistory(int vehicle_id);
    std::vector<Customer> searchCustomers(const std::string& field, const std::string& value);
    std::vector<RegisterRow> getAccountRegister(int account_id);

    // Print Template operations
    std::vector<PrintTemplate> getPrintTemplates();
    bool addPrintTemplate(const PrintTemplate& tmpl);
    bool updatePrintTemplate(const PrintTemplate& tmpl);
    bool deletePrintTemplate(int id);
    bool setActivePrintTemplate(int id);
    PrintTemplate getActivePrintTemplate();

    // Settings persistence
    std::string getSetting(const std::string& key, const std::string& default_value = "");
    bool setSetting(const std::string& key, const std::string& value);

private:
    sqlite3* m_db;
    std::recursive_mutex m_mutex;

    bool executeSQL(const std::string& sql);
    bool seedDefaultTemplates();
    bool addPrintTemplateInternal(const PrintTemplate& tmpl);
    bool initializeSchema();
    int getAccountIdByName(const std::string& name);
};

} // namespace tuxrepair

#endif // TUXREPAIR_DB_MANAGER_H
