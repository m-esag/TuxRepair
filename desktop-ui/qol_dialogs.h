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
    void setInitialSearchField(const QString& field_name);

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

// --- Quick Payment & Finalization Dialog ---
class QuickPaymentDialog : public QDialog {
    Q_OBJECT
public:
    explicit QuickPaymentDialog(int invoice_id, double amount_due, std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

    std::string paymentMethod() const { return m_payment_method; }
    double amountPaid() const { return m_amount_paid; }

private slots:
    void onProcessPayment();

private:
    std::shared_ptr<DBManager> m_db;
    int m_invoice_id;
    double m_amount_due;
    std::string m_payment_method = "Cash";
    double m_amount_paid = 0.0;

    QComboBox* m_method_combo;
    QLineEdit* m_amount_edit;
    QLabel* m_change_lbl;
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

// --- Signature Pad Dialog ---
class SignaturePadDialog : public QDialog {
    Q_OBJECT
public:
    explicit SignaturePadDialog(QWidget* parent = nullptr);
    QString signatureBase64() const { return m_sig_base64; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onClear();
    void onAccept();

private:
    QList<QList<QPoint>> m_strokes;
    QString m_sig_base64;
    QPushButton* m_clear_btn;
    QPushButton* m_ok_btn;
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

// --- New Intake Wizard Dialog ---
class NewIntakeWizardDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewIntakeWizardDialog(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

    int customerId() const { return m_created_customer_id; }
    int vehicleId() const { return m_created_vehicle_id; }

private slots:
    void onCompleteIntake();

private:
    std::shared_ptr<DBManager> m_db;
    int m_created_customer_id = -1;
    int m_created_vehicle_id = -1;

    QLineEdit* m_first_name_edit;
    QLineEdit* m_last_name_edit;
    QLineEdit* m_phone_edit;
    QLineEdit* m_email_edit;
    QLineEdit* m_plate_edit;
    QLineEdit* m_year_edit;
    QLineEdit* m_model_edit;
    QLineEdit* m_engine_edit;
    QLineEdit* m_mileage_edit;
};

// --- Work-In-Progress (WIP) Live Dashboard Dialog ---
class WipDashboardDialog : public QDialog {
    Q_OBJECT
public:
    explicit WipDashboardDialog(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

    int selectedInvoiceId() const { return m_selected_inv_id; }

private slots:
    void refreshDashboard();
    void onOpenTicket();
    void onChangeStatus(const QString& new_status);

private:
    std::shared_ptr<DBManager> m_db;
    int m_selected_inv_id = -1;

    QTableWidget* m_wip_table;
    QLabel* m_cnt_new_lbl;
    QLabel* m_cnt_in_progress_lbl;
    QLabel* m_cnt_waiting_parts_lbl;
    QLabel* m_cnt_ready_lbl;
};

// --- Custom PDF & Printing Settings Dialog ---
class CustomPdfSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomPdfSettingsDialog(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

private slots:
    void onSaveSettings();

private:
    std::shared_ptr<DBManager> m_db;

    QLineEdit* m_header_text_edit;
    QLineEdit* m_footer_disclaimer_edit;
    QLineEdit* m_logo_path_edit;
};

} // namespace tuxrepair

#endif // TUXREPAIR_QOL_DIALOGS_H
