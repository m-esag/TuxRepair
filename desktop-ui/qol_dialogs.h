#ifndef TUXREPAIR_QOL_DIALOGS_H
#define TUXREPAIR_QOL_DIALOGS_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <memory>
#include "db_manager.h"
#include "models.h"

namespace tuxrepair {

// --- Customer Lookup Dialog ---
class CustomerLookupDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomerLookupDialog(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);
    
    bool hasSelection() const { return m_has_selection; }
    Customer selectedCustomer() const { return m_selected_customer; }
    Vehicle selectedVehicle() const { return m_selected_vehicle; }

private slots:
    void onSearch();
    void onSelect();

private:
    std::shared_ptr<DBManager> m_db;
    bool m_has_selection = false;
    Customer m_selected_customer;
    Vehicle m_selected_vehicle;

    QLineEdit* m_search_edit;
    QComboBox* m_field_combo;
    QTableWidget* m_results_table;
    QPushButton* m_select_btn;
};

// --- Catalog Lookup Dialog ---
class CatalogLookupDialog : public QDialog {
    Q_OBJECT
public:
    explicit CatalogLookupDialog(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);
    
    bool hasSelection() const { return m_has_selection; }
    QString selectedType() const { return m_selected_type; }
    QString selectedCode() const { return m_selected_code; }
    QString selectedDescription() const { return m_selected_desc; }
    double selectedPrice() const { return m_selected_price; }

private slots:
    void onSearch();
    void onSelect();

private:
    std::shared_ptr<DBManager> m_db;
    bool m_has_selection = false;
    QString m_selected_type;
    QString m_selected_code;
    QString m_selected_desc;
    double m_selected_price = 0.0;

    QLineEdit* m_search_edit;
    QRadioButton* m_radio_all;
    QRadioButton* m_radio_parts;
    QRadioButton* m_radio_labor;
    QTableWidget* m_catalog_table;
    QPushButton* m_select_btn;
};

// --- Service History Dialog ---
class ServiceHistoryDialog : public QDialog {
    Q_OBJECT
public:
    ServiceHistoryDialog(int vehicle_id, const QString& vehicle_info, std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

private:
    std::shared_ptr<DBManager> m_db;
    QTableWidget* m_history_table;
    QTableWidget* m_details_table;

    void loadHistory(int vehicle_id);
    void onVisitSelected(int row);
};

// --- General Invoice / Quotes Lookup Dialog ---
class InvoiceLookupDialog : public QDialog {
    Q_OBJECT
public:
    InvoiceLookupDialog(bool show_quotes_only, std::shared_ptr<DBManager> db, QWidget* parent = nullptr);
    int selectedInvoiceId() const { return m_selected_invoice_id; }

private slots:
    void onSearch();
    void onSelect();

private:
    bool m_show_quotes_only;
    std::shared_ptr<DBManager> m_db;
    int m_selected_invoice_id;

    QLineEdit* m_search_edit;
    QTableWidget* m_table;
    QPushButton* m_select_btn;
};

// --- Add Vehicle Dialog ---
class AddVehicleDialog : public QDialog {
    Q_OBJECT
public:
    AddVehicleDialog(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

    int year() const;
    std::string make() const;
    std::string model() const;
    std::string licensePlate() const;
    std::string engineSpecs() const;

private slots:
    void onMakeChanged(const QString& make);
    void onModelChanged(const QString& model);
    void onSave();

private:
    std::shared_ptr<DBManager> m_db;
    QComboBox* m_year_combo;
    QComboBox* m_make_combo;
    QComboBox* m_model_combo;
    QLineEdit* m_plate_edit;
    QLineEdit* m_engine_edit;
};

} // namespace tuxrepair

#endif // TUXREPAIR_QOL_DIALOGS_H
