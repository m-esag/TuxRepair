#ifndef TUXREPAIR_MAIN_WINDOW_H
#define TUXREPAIR_MAIN_WINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>
#include <memory>
#include "db_manager.h"
#include "models.h"

namespace tuxrepair {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // Rapid Intake Tab slots
    void onPlateSearchTextChanged(const QString& text);
    void onPlateSearchReturnPressed();
    void onSelectIntakeVehicle(int row);

    // Tickets Tab slots
    void refreshInvoicesList();
    void onInvoiceSelected(int row);
    void onAddPartItem();
    void onAddLaborItem();
    void onRemoveSelectedItems();
    void onSaveInvoiceChanges();
    void clearActiveInvoiceView();
    void onAddNewVehicleToCustomer();
    void onPrintTraveler();
    void onFinalizeInvoice();
    void onInsertJobKit();
    void recalculateTicketTotals();
    void onItemsTableContextMenu(const QPoint& pos);
    void onItemsTableCellClicked(int row, int col);

    // Accounting Tab slots
    void refreshAccountingData();

    // Inventory Tab slots
    void refreshInventoryData();
    void onAddInventory();
    void onInventorySelected(int row);

    // Scheduler Tab slots
    void refreshSchedulerData();
    void onAddSchedule();
    void onDeleteSchedule();

private:
    std::shared_ptr<DBManager> m_db;
    
    // Active states
    int m_selected_intake_vehicle_id = -1;
    int m_selected_intake_customer_id = -1;
    int m_active_invoice_id = -1;
    bool m_is_dirty = false;
    bool m_supplies_removed = false;
    double m_sales_tax_rate = 0.08;
    double m_supplies_percent = 0.10; // 10%
    int64_t m_supplies_cap_cents = 3500; // $35.00
    bool m_dark_theme = false;
    void applyTheme();
    void markDirty() { m_is_dirty = true; }
    void setupMenuBar();

    // Tabs
    QTabWidget* m_tab_widget;
    QWidget* m_tickets_tab;
    QWidget* m_ledger_tab;
    QWidget* m_inventory_tab;
    QWidget* m_scheduler_tab;

    // --- Rapid Intake UI Elements ---
    QLineEdit* m_search_plate_edit;
    QCompleter* m_search_completer;
    QTableWidget* m_intake_results_table;


    // --- Tickets UI Elements ---
    QTableWidget* m_invoices_table;
    QTabWidget* m_tickets_left_tabs;
    
    // Active Ticket details
    QLabel* m_ticket_title_label;
    QComboBox* m_ticket_type_combo;
    QLineEdit* m_ticket_mileage_in_edit;
    QLineEdit* m_ticket_mileage_out_edit;
    QLineEdit* m_billed_by_edit;
    QLabel* m_ticket_status_label;
    
    // Edit details side-by-side
    QLineEdit* m_t_cust_first_edit;
    QLineEdit* m_t_cust_last_edit;
    QLineEdit* m_t_cust_phone_edit;
    QLineEdit* m_t_veh_plate_edit;
    QLineEdit* m_t_veh_model_edit;
    QLineEdit* m_t_veh_engine_edit;

    QTableWidget* m_items_table;
    QPushButton* m_add_part_btn;
    QPushButton* m_add_labor_btn;
    QPushButton* m_remove_item_btn;
    QPushButton* m_insert_job_kit_btn;
    QPushButton* m_insert_catalog_btn;
    QPushButton* m_move_up_btn;
    QPushButton* m_move_down_btn;
    
    QPushButton* m_finalize_ticket_btn;
    QSpinBox* m_nav_invoice_spin;
    QTableWidget* m_history_table;

    // Bottom summary labels
    QLabel* m_summary_parts_lbl;
    QLabel* m_summary_labor_lbl;
    QLabel* m_summary_supplies_lbl;
    QLabel* m_summary_subtotal_lbl;
    QLabel* m_summary_tax_lbl;
    QLabel* m_summary_tax_title_lbl;
    QLabel* m_summary_total_lbl;

    // --- Accounting UI Elements ---
    QTableWidget* m_accounts_table;
    QTableWidget* m_transactions_table;
    QPushButton* m_export_ledger_btn;

    // --- Inventory UI Elements ---
    QTableWidget* m_inventory_table;
    QLineEdit* m_part_num_edit;
    QLineEdit* m_part_desc_edit;
    QLineEdit* m_part_qty_edit;
    QLineEdit* m_part_reorder_edit;
    QLineEdit* m_part_cost_edit;
    QLineEdit* m_part_retail_edit;
    QPushButton* m_add_inventory_btn;
    QPushButton* m_update_inventory_btn;

    // --- Scheduler UI Elements ---
    QTableWidget* m_scheduler_table;
    QComboBox* m_schedule_bay_combo;
    QLineEdit* m_schedule_cust_edit;
    QLineEdit* m_schedule_veh_edit;
    QLineEdit* m_schedule_time_edit;
    QLineEdit* m_schedule_notes_edit;
    QPushButton* m_add_schedule_btn;
    QPushButton* m_delete_schedule_btn;

    // Init helpers
    void setupTicketsTab();
    void setupLedgerTab();
    void setupInventoryTab();
    void setupSchedulerTab();

    // Helper functions
    void loadInvoiceDetails(int invoice_id);
    void updateSearchCompleter();
    void onMoveItemUp();
    void onMoveItemDown();
    void onExportLedgerToCSV();
    QString formatCents(int64_t cents);
};

} // namespace tuxrepair

#endif // TUXREPAIR_MAIN_WINDOW_H
