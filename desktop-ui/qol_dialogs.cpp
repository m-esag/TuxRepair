#include "qol_dialogs.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <QImage>
#include <QBuffer>

namespace tuxrepair {

// ==========================================
// CUSTOMER LOOKUP DIALOG
// ==========================================
CustomerLookupDialog::CustomerLookupDialog(std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle("Find A Customer / Vehicle");
    resize(700, 450);

    auto layout = new QVBoxLayout(this);

    auto search_layout = new QHBoxLayout();
    m_search_edit = new QLineEdit(this);
    m_search_edit->setPlaceholderText("Enter search term...");
    search_layout->addWidget(m_search_edit);

    m_field_combo = new QComboBox(this);
    m_field_combo->addItems({"License Plate", "Last Name", "First Name", "Phone"});
    search_layout->addWidget(m_field_combo);

    auto search_btn = new QPushButton("Search", this);
    search_layout->addWidget(search_btn);
    layout->addLayout(search_layout);

    m_results_table = new QTableWidget(this);
    m_results_table->setColumnCount(5);
    m_results_table->setHorizontalHeaderLabels({"Customer Name", "Phone", "Plate", "Year / Model", "Engine"});
    m_results_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_results_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_results_table);

    auto btn_layout = new QHBoxLayout();
    m_select_btn = new QPushButton("Select & Intake", this);
    m_select_btn->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 6px;");
    auto cancel_btn = new QPushButton("Cancel", this);
    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(m_select_btn);
    layout->addLayout(btn_layout);

    connect(m_search_edit, &QLineEdit::textChanged, this, &CustomerLookupDialog::onSearch);
    connect(m_field_combo, &QComboBox::currentIndexChanged, this, &CustomerLookupDialog::onSearch);
    connect(search_btn, &QPushButton::clicked, this, &CustomerLookupDialog::onSearch);
    connect(m_select_btn, &QPushButton::clicked, this, &CustomerLookupDialog::onSelect);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_results_table, &QTableWidget::cellDoubleClicked, this, [this](int, int){ onSelect(); });

    // Initial load
    onSearch();
}

void CustomerLookupDialog::setInitialSearchField(const QString& field_name) {
    if (field_name == "License") {
        m_field_combo->setCurrentText("License Plate");
    } else if (field_name == "First Name") {
        m_field_combo->setCurrentText("First Name");
    } else if (field_name == "Phone") {
        m_field_combo->setCurrentText("Phone");
    } else {
        m_field_combo->setCurrentText("Last Name");
    }
    m_search_edit->setFocus();
}

struct CustomerResultRow {
    Customer customer;
    Vehicle vehicle;
    bool has_vehicle = false;
};

void CustomerLookupDialog::onSearch() {
    std::string text = m_search_edit->text().trimmed().toStdString();
    int mode = m_field_combo->currentIndex(); // 0: License Plate, 1: Last Name, 2: First Name, 3: Phone

    std::vector<CustomerResultRow> rows;

    if (mode == 0) { // License Plate
        auto vehicles = m_db->searchVehiclesByPlate(text.empty() ? "%" : text);
        for (const auto& v : vehicles) {
            Customer c;
            m_db->getCustomer(v.customer_id, c);
            CustomerResultRow r;
            r.customer = c;
            r.vehicle = v;
            r.has_vehicle = true;
            rows.push_back(r);
        }
        std::sort(rows.begin(), rows.end(), [](const CustomerResultRow& a, const CustomerResultRow& b) {
            return a.vehicle.license_plate < b.vehicle.license_plate;
        });
    } else {
        std::string db_field = "last_name";
        if (mode == 2) db_field = "first_name";
        else if (mode == 3) db_field = "phone_number";

        auto customers = m_db->searchCustomers(db_field, text);
        auto all_vehicles = m_db->searchVehiclesByPlate("%");

        for (const auto& c : customers) {
            std::vector<Vehicle> matched_vehs;
            for (const auto& v : all_vehicles) {
                if (v.customer_id == c.id) matched_vehs.push_back(v);
            }
            if (matched_vehs.empty()) {
                CustomerResultRow r;
                r.customer = c;
                r.has_vehicle = false;
                rows.push_back(r);
            } else {
                for (const auto& v : matched_vehs) {
                    CustomerResultRow r;
                    r.customer = c;
                    r.vehicle = v;
                    r.has_vehicle = true;
                    rows.push_back(r);
                }
            }
        }

        if (mode == 1) { // Last Name
            std::sort(rows.begin(), rows.end(), [](const CustomerResultRow& a, const CustomerResultRow& b) {
                if (a.customer.last_name != b.customer.last_name) return a.customer.last_name < b.customer.last_name;
                return a.customer.first_name < b.customer.first_name;
            });
        } else if (mode == 2) { // First Name
            std::sort(rows.begin(), rows.end(), [](const CustomerResultRow& a, const CustomerResultRow& b) {
                if (a.customer.first_name != b.customer.first_name) return a.customer.first_name < b.customer.first_name;
                return a.customer.last_name < b.customer.last_name;
            });
        } else if (mode == 3) { // Phone (Focus on Last 4 Digits)
            auto get_last4 = [](const std::string& p) -> std::string {
                std::string digits;
                for (char ch : p) {
                    if (std::isdigit(static_cast<unsigned char>(ch))) digits += ch;
                }
                return digits.size() >= 4 ? digits.substr(digits.size() - 4) : digits;
            };
            std::sort(rows.begin(), rows.end(), [get_last4](const CustomerResultRow& a, const CustomerResultRow& b) {
                std::string last4_a = get_last4(a.customer.phone_number);
                std::string last4_b = get_last4(b.customer.phone_number);
                if (last4_a != last4_b) return last4_a < last4_b;
                return a.customer.phone_number < b.customer.phone_number;
            });
        }
    }

    m_results_table->setRowCount(0);
    for (const auto& r : rows) {
        int row = m_results_table->rowCount();
        m_results_table->insertRow(row);

        auto name_item = new QTableWidgetItem(QString("%1, %2")
                                                  .arg(QString::fromStdString(r.customer.last_name))
                                                  .arg(QString::fromStdString(r.customer.first_name)));
        auto phone_item = new QTableWidgetItem(QString::fromStdString(r.customer.phone_number));
        QTableWidgetItem* plate_item = nullptr;
        QTableWidgetItem* model_item = nullptr;
        QTableWidgetItem* engine_item = nullptr;

        if (r.has_vehicle) {
            plate_item = new QTableWidgetItem(QString::fromStdString(r.vehicle.license_plate));
            model_item = new QTableWidgetItem(QString("%1 %2").arg(r.vehicle.year).arg(QString::fromStdString(r.vehicle.model)));
            engine_item = new QTableWidgetItem(QString::fromStdString(r.vehicle.engine_specs));
        } else {
            plate_item = new QTableWidgetItem("N/A");
            model_item = new QTableWidgetItem("New Customer Record");
            engine_item = new QTableWidgetItem("N/A");
        }

        int c_id = r.customer.id;
        int v_id = r.has_vehicle ? r.vehicle.id : -1;

        for (auto* item : {name_item, phone_item, plate_item, model_item, engine_item}) {
            item->setData(Qt::UserRole, c_id);
            item->setData(Qt::UserRole + 1, v_id);
        }

        m_results_table->setItem(row, 0, name_item);
        m_results_table->setItem(row, 1, phone_item);
        m_results_table->setItem(row, 2, plate_item);
        m_results_table->setItem(row, 3, model_item);
        m_results_table->setItem(row, 4, engine_item);
    }
}

void CustomerLookupDialog::onSelect() {
    int row = m_results_table->currentRow();
    if (row < 0 || row >= m_results_table->rowCount()) {
        auto ranges = m_results_table->selectedRanges();
        if (ranges.isEmpty()) {
            QMessageBox::warning(this, "No Selection", "Please select a customer row from the results table.");
            return;
        }
        row = ranges.first().topRow();
    }

    auto name_item = m_results_table->item(row, 0);
    if (!name_item) {
        QMessageBox::warning(this, "Selection Error", "Invalid table row selected.");
        return;
    }

    int cust_id = name_item->data(Qt::UserRole).toInt();
    int veh_id = name_item->data(Qt::UserRole + 1).toInt();

    m_selected_customer = Customer{};
    m_selected_vehicle = Vehicle{};

    m_db->getCustomer(cust_id, m_selected_customer);
    if (veh_id != -1) {
        m_db->getVehicle(veh_id, m_selected_vehicle);
    } else {
        m_selected_vehicle.id = -1;
        m_selected_vehicle.customer_id = cust_id;
    }
    m_has_selection = true;

    accept();
}


// ==========================================
// CATALOG LOOKUP DIALOG
// ==========================================
CatalogLookupDialog::CatalogLookupDialog(std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle("Parts & Labor Catalog");
    resize(650, 450);

    auto layout = new QVBoxLayout(this);

    auto filter_layout = new QHBoxLayout();
    m_search_edit = new QLineEdit(this);
    m_search_edit->setPlaceholderText("Filter catalog items...");
    filter_layout->addWidget(m_search_edit);

    m_radio_all = new QRadioButton("All Only", this);
    m_radio_parts = new QRadioButton("Parts Only", this);
    m_radio_labor = new QRadioButton("Labor Only", this);
    m_radio_all->setChecked(true);
    filter_layout->addWidget(m_radio_all);
    filter_layout->addWidget(m_radio_parts);
    filter_layout->addWidget(m_radio_labor);

    auto search_btn = new QPushButton("Filter", this);
    filter_layout->addWidget(search_btn);
    layout->addLayout(filter_layout);

    m_catalog_table = new QTableWidget(this);
    m_catalog_table->setColumnCount(5);
    m_catalog_table->setHorizontalHeaderLabels({"Type", "SKU / Code", "Description", "QOH", "Unit Retail Price"});
    m_catalog_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_catalog_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_catalog_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_catalog_table);

    auto btn_layout = new QHBoxLayout();
    m_select_btn = new QPushButton("Insert Into Invoice", this);
    m_select_btn->setStyleSheet("background-color: #1565c0; color: white; font-weight: bold; padding: 6px;");
    auto cancel_btn = new QPushButton("Cancel", this);
    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(m_select_btn);
    layout->addLayout(btn_layout);

    connect(search_btn, &QPushButton::clicked, this, &CatalogLookupDialog::onSearch);
    connect(m_select_btn, &QPushButton::clicked, this, &CatalogLookupDialog::onSelect);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_catalog_table, &QTableWidget::cellDoubleClicked, this, [this](int, int){ onSelect(); });

    onSearch();
}

void CatalogLookupDialog::onSearch() {
    QString filter = m_search_edit->text().trimmed().toLower();
    m_catalog_table->setRowCount(0);

    // Get stock parts
    if (!m_radio_labor->isChecked()) {
        auto stock = m_db->getInventory();
        for (const auto& item : stock) {
            QString code = QString::fromStdString(item.part_number);
            QString desc = QString::fromStdString(item.description);
            if (!filter.isEmpty() && !code.toLower().contains(filter) && !desc.toLower().contains(filter)) continue;

            int row = m_catalog_table->rowCount();
            m_catalog_table->insertRow(row);

            m_catalog_table->setItem(row, 0, new QTableWidgetItem("Part"));
            m_catalog_table->setItem(row, 1, new QTableWidgetItem(code));
            m_catalog_table->setItem(row, 2, new QTableWidgetItem(desc));
            
            auto qoh_item = new QTableWidgetItem(QString::number(item.quantity_on_hand, 'f', 0));
            if (item.quantity_on_hand <= 0) {
                qoh_item->setBackground(QBrush(QColor("#ffcdd2"))); // Red warning
                qoh_item->setText(qoh_item->text() + " (OUT OF STOCK)");
            } else if (item.quantity_on_hand <= item.reorder_point) {
                qoh_item->setBackground(QBrush(QColor("#fff9c4"))); // Yellow warning
                qoh_item->setText(qoh_item->text() + " (LOW STOCK)");
            }
            m_catalog_table->setItem(row, 3, qoh_item);

            double d_price = item.retail_price / 100.0;
            m_catalog_table->setItem(row, 4, new QTableWidgetItem(QString::number(d_price, 'f', 2)));
        }
    }

    // Add standard labor templates
    if (!m_radio_parts->isChecked()) {
        struct LaborTemplate {
            QString code;
            QString desc;
            double price;
        };
        std::vector<LaborTemplate> labors = {
            {"LABOR-DIAG", "General Diagnostic Fee", 85.00},
            {"LABOR-OIL", "Standard Oil & Filter Lube Labor", 25.00},
            {"LABOR-BRAKE", "Front Disc Brake Pad Service Labor", 65.00},
            {"LABOR-TUNE", "Standard 4-Cylinder Tune Up Labor", 120.00},
            {"LABOR-ALIGN", "4-Wheel Precision Alignment Labor", 89.95}
        };

        for (const auto& l : labors) {
            if (!filter.isEmpty() && !l.code.toLower().contains(filter) && !l.desc.toLower().contains(filter)) continue;

            int row = m_catalog_table->rowCount();
            m_catalog_table->insertRow(row);

            m_catalog_table->setItem(row, 0, new QTableWidgetItem("Labor"));
            m_catalog_table->setItem(row, 1, new QTableWidgetItem(l.code));
            m_catalog_table->setItem(row, 2, new QTableWidgetItem(l.desc));
            m_catalog_table->setItem(row, 3, new QTableWidgetItem("N/A"));
            m_catalog_table->setItem(row, 4, new QTableWidgetItem(QString::number(l.price, 'f', 2)));
        }
    }
}

// ==========================================
// QUICK PAYMENT & FINALIZATION DIALOG
// ==========================================
QuickPaymentDialog::QuickPaymentDialog(int invoice_id, double amount_due, std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db), m_invoice_id(invoice_id), m_amount_due(amount_due) {
    setWindowTitle(QString("Process Payment - Work Order #%1").arg(invoice_id));
    resize(420, 280);

    auto layout = new QVBoxLayout(this);

    auto due_lbl = new QLabel(QString("Total Amount Due: $%1").arg(QString::number(amount_due, 'f', 2)), this);
    due_lbl->setStyleSheet("font-weight: bold; font-size: 16px; color: #2e7d32; padding: 4px;");
    layout->addWidget(due_lbl);

    auto grid = new QGridLayout();
    grid->addWidget(new QLabel("Payment Method:", this), 0, 0);
    m_method_combo = new QComboBox(this);
    m_method_combo->addItems({"Cash", "Credit Card", "Debit Card", "Check", "Split Payment"});
    grid->addWidget(m_method_combo, 0, 1);

    grid->addWidget(new QLabel("Amount Tended / Paid ($):", this), 1, 0);
    m_amount_edit = new QLineEdit(this);
    m_amount_edit->setText(QString::number(amount_due, 'f', 2));
    grid->addWidget(m_amount_edit, 1, 1);

    m_change_lbl = new QLabel("Change Due: $0.00", this);
    m_change_lbl->setStyleSheet("font-weight: bold; font-size: 13px; color: blue;");
    grid->addWidget(m_change_lbl, 2, 1);

    layout->addLayout(grid);

    auto update_change = [this]() {
        double paid = m_amount_edit->text().toDouble();
        double change = paid - m_amount_due;
        if (change < 0.0) change = 0.0;
        m_change_lbl->setText(QString("Change Due: $%1").arg(QString::number(change, 'f', 2)));
    };

    connect(m_amount_edit, &QLineEdit::textChanged, this, update_change);

    auto btn_layout = new QHBoxLayout();
    auto pay_btn = new QPushButton("💳 Finalize & Pay (F12)", this);
    pay_btn->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 8px; font-size: 13px;");
    auto cancel_btn = new QPushButton("Cancel", this);

    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(pay_btn);
    layout->addLayout(btn_layout);

    connect(pay_btn, &QPushButton::clicked, this, &QuickPaymentDialog::onProcessPayment);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
}

void QuickPaymentDialog::onProcessPayment() {
    m_payment_method = m_method_combo->currentText().toStdString();
    m_amount_paid = m_amount_edit->text().toDouble();

    if (m_amount_paid < m_amount_due && m_payment_method != "Split Payment") {
        QMessageBox::warning(this, "Insufficient Payment", "Payment amount tendering is less than the total amount due.");
        return;
    }

    accept();
}

void CatalogLookupDialog::onSelect() {
    auto ranges = m_catalog_table->selectedRanges();
    if (ranges.isEmpty()) return;

    int row = ranges.first().topRow();
    m_selected_type = m_catalog_table->item(row, 0)->text();
    m_selected_code = m_catalog_table->item(row, 1)->text();
    m_selected_desc = m_catalog_table->item(row, 2)->text();
    m_selected_price = m_catalog_table->item(row, 4)->text().toDouble();
    m_has_selection = true;

    accept();
}


// ==========================================
// SERVICE HISTORY DIALOG
// ==========================================
ServiceHistoryDialog::ServiceHistoryDialog(int vehicle_id, const QString& vehicle_info, std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle(QString("Service History - %1").arg(vehicle_info));
    resize(750, 480);

    auto layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel("Select a previous repair visit (invoice) below:", this));
    
    m_history_table = new QTableWidget(this);
    m_history_table->setColumnCount(4);
    m_history_table->setHorizontalHeaderLabels({"Invoice ID", "Date", "Odometer", "Status"});
    m_history_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_history_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_history_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_history_table);

    layout->addWidget(new QLabel("Invoice Parts and Labor Details:", this));
    m_details_table = new QTableWidget(this);
    m_details_table->setColumnCount(4);
    m_details_table->setHorizontalHeaderLabels({"Type", "Description", "Quantity", "Total Price"});
    m_details_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_details_table);

    auto exit_btn = new QPushButton("Exit", this);
    exit_btn->setStyleSheet("padding: 6px;");
    layout->addWidget(exit_btn);

    connect(m_history_table, &QTableWidget::cellClicked, this, [this](int row, int){ onVisitSelected(row); });
    connect(exit_btn, &QPushButton::clicked, this, &QDialog::accept);

    loadHistory(vehicle_id);
}

void ServiceHistoryDialog::loadHistory(int vehicle_id) {
    auto visits = m_db->getVehicleServiceHistory(vehicle_id);
    m_history_table->setRowCount(0);
    m_details_table->setRowCount(0);

    for (const auto& inv : visits) {
        int row = m_history_table->rowCount();
        m_history_table->insertRow(row);

        auto id_item = new QTableWidgetItem(QString::number(inv.id));
        m_history_table->setItem(row, 0, id_item);
        m_history_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(inv.date_created)));
        m_history_table->setItem(row, 2, new QTableWidgetItem(QString("%1 miles").arg(inv.mileage_in)));
        m_history_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(inv.status)));
    }
}

void ServiceHistoryDialog::onVisitSelected(int row) {
    int invoice_id = m_history_table->item(row, 0)->text().toInt();
    
    Invoice inv;
    m_details_table->setRowCount(0);
    if (m_db->getInvoice(invoice_id, inv)) {
        for (const auto& line : inv.items) {
            int row_d = m_details_table->rowCount();
            m_details_table->insertRow(row_d);

            m_details_table->setItem(row_d, 0, new QTableWidgetItem(QString::fromStdString(line.specification)));
            m_details_table->setItem(row_d, 1, new QTableWidgetItem(QString::fromStdString(line.description)));
            m_details_table->setItem(row_d, 2, new QTableWidgetItem(QString::number(line.quantity)));
            
            double d_total = (line.quantity * line.unit_price) / 100.0;
            m_details_table->setItem(row_d, 3, new QTableWidgetItem(QString("$%1").arg(QString::number(d_total, 'f', 2))));
        }
    }
}

// --- InvoiceLookupDialog Implementation ---
InvoiceLookupDialog::InvoiceLookupDialog(bool show_quotes_only, std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_show_quotes_only(show_quotes_only), m_db(db), m_selected_invoice_id(-1) {
    
    setWindowTitle(show_quotes_only ? "Quotes & Estimates Lookup" : "Vehicle Service History Lookup");
    resize(700, 450);

    auto layout = new QVBoxLayout(this);

    // Search bar
    auto search_lay = new QHBoxLayout();
    m_search_edit = new QLineEdit(this);
    m_search_edit->setPlaceholderText("Search by Customer Name, Plate, Model, or ID...");
    m_search_edit->setStyleSheet("font-size: 13px; padding: 4px;");
    auto search_btn = new QPushButton("Search", this);
    search_btn->setStyleSheet("padding: 4px 12px; font-weight: bold;");
    search_lay->addWidget(m_search_edit);
    search_lay->addWidget(search_btn);
    layout->addLayout(search_lay);

    // Invoices list table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"ID", "Date", "Customer", "Plate / Model", "Type", "Status"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table);

    // Actions
    auto btn_layout = new QHBoxLayout();
    m_select_btn = new QPushButton("Open Selected", this);
    m_select_btn->setStyleSheet("background-color: #1565c0; color: white; font-weight: bold; padding: 6px 16px;");
    auto cancel_btn = new QPushButton("Cancel", this);
    cancel_btn->setStyleSheet("padding: 6px 16px;");
    btn_layout->addStretch();
    btn_layout->addWidget(m_select_btn);
    btn_layout->addWidget(cancel_btn);
    layout->addLayout(btn_layout);

    // Connections
    connect(search_btn, &QPushButton::clicked, this, &InvoiceLookupDialog::onSearch);
    connect(m_search_edit, &QLineEdit::textChanged, this, &InvoiceLookupDialog::onSearch);
    connect(m_select_btn, &QPushButton::clicked, this, &InvoiceLookupDialog::onSelect);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int){ onSelect(); });

    onSearch();
}

void InvoiceLookupDialog::onSearch() {
    QString filter = m_search_edit->text().trimmed().toLower();
    m_table->setRowCount(0);

    auto list = m_db->getAllInvoices();
    for (const auto& inv : list) {
        // Quotes/Estimates: ticket type is Estimate or Quote AND status is Open
        bool is_quote = (inv.ticket_type == "Quote" || inv.ticket_type == "Estimate");
        if (m_show_quotes_only && !is_quote) continue;
        if (!m_show_quotes_only && is_quote) continue; // Service history shows actual finalized jobs

        QString id_str = QString::number(inv.id);
        QString date_str = QString::fromStdString(inv.date_created);
        QString name = QString("%1, %2").arg(QString::fromStdString(inv.customer.last_name)).arg(QString::fromStdString(inv.customer.first_name));
        QString plate_model = QString("%1 (%2)").arg(QString::fromStdString(inv.vehicle.license_plate)).arg(QString::fromStdString(inv.vehicle.model));
        QString type_str = QString::fromStdString(inv.ticket_type);
        QString status_str = QString::fromStdString(inv.status);

        if (!filter.isEmpty() &&
            !id_str.contains(filter) &&
            !date_str.contains(filter) &&
            !name.toLower().contains(filter) &&
            !plate_model.toLower().contains(filter) &&
            !type_str.toLower().contains(filter) &&
            !status_str.toLower().contains(filter)) {
            continue;
        }

        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(id_str));
        m_table->setItem(row, 1, new QTableWidgetItem(date_str));
        m_table->setItem(row, 2, new QTableWidgetItem(name));
        m_table->setItem(row, 3, new QTableWidgetItem(plate_model));
        m_table->setItem(row, 4, new QTableWidgetItem(type_str));
        m_table->setItem(row, 5, new QTableWidgetItem(status_str));
    }
}

void InvoiceLookupDialog::onSelect() {
    int row = m_table->currentRow();
    if (row >= 0) {
        m_selected_invoice_id = m_table->item(row, 0)->text().toInt();
        accept();
    } else {
        QMessageBox::warning(this, "No Selection", "Please select a record from the list first.");
    }
}


// --- AddVehicleDialog Implementation ---
AddVehicleDialog::AddVehicleDialog(std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    
    setWindowTitle("Add New Vehicle");
    resize(380, 260);

    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    m_plate_edit = new QLineEdit(this);
    m_plate_edit->setPlaceholderText("Enter plate e.g. 12345");
    
    m_year_combo = new QComboBox(this);
    for (int y = 2027; y >= 1990; --y) {
        m_year_combo->addItem(QString::number(y));
    }
    m_year_combo->setCurrentText("2018");

    m_make_combo = new QComboBox(this);
    auto db_makes = m_db->getCarMakes();
    if (db_makes.empty()) {
        m_make_combo->addItems({
            "Ford", "Toyota", "Honda", "Chevrolet", "Nissan", 
            "Dodge", "Jeep", "GMC", "Subaru", "Hyundai"
        });
    } else {
        for (const auto& make : db_makes) {
            m_make_combo->addItem(QString::fromStdString(make));
        }
    }

    m_model_combo = new QComboBox(this);

    m_engine_edit = new QLineEdit(this);
    m_engine_edit->setPlaceholderText("e.g. 2.5L I4, 3.5L V6");

    form->addRow("License Plate:", m_plate_edit);
    form->addRow("Year:", m_year_combo);
    form->addRow("Make/Brand:", m_make_combo);
    form->addRow("Model:", m_model_combo);
    form->addRow("Engine Specs:", m_engine_edit);
    layout->addLayout(form);

    // Dialog buttons
    auto btn_layout = new QHBoxLayout();
    auto save_btn = new QPushButton("Save Vehicle", this);
    save_btn->setStyleSheet("background-color: #4caf50; color: white; font-weight: bold; padding: 6px 12px;");
    auto cancel_btn = new QPushButton("Cancel", this);
    cancel_btn->setStyleSheet("padding: 6px 12px;");
    btn_layout->addStretch();
    btn_layout->addWidget(save_btn);
    btn_layout->addWidget(cancel_btn);
    layout->addLayout(btn_layout);

    // Connections
    connect(m_make_combo, &QComboBox::currentTextChanged, this, &AddVehicleDialog::onMakeChanged);
    connect(m_model_combo, &QComboBox::currentTextChanged, this, &AddVehicleDialog::onModelChanged);
    connect(save_btn, &QPushButton::clicked, this, &AddVehicleDialog::onSave);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);

    // Trigger first initialization
    onMakeChanged(m_make_combo->currentText());
}

void AddVehicleDialog::onMakeChanged(const QString& make) {
    m_model_combo->clear();
    auto db_models = m_db->getCarModels(make.toStdString());
    if (db_models.empty()) {
        if (make == "Ford") {
            m_model_combo->addItems({"F-150", "Explorer", "Escape", "Mustang", "Fusion"});
        } else if (make == "Toyota") {
            m_model_combo->addItems({"Camry", "Corolla", "RAV4", "Tacoma", "Highlander"});
        } else if (make == "Honda") {
            m_model_combo->addItems({"Civic", "Accord", "CR-V", "Pilot", "Odyssey"});
        } else if (make == "Chevrolet") {
            m_model_combo->addItems({"Silverado", "Equinox", "Tahoe", "Malibu", "Cruze"});
        } else if (make == "Nissan") {
            m_model_combo->addItems({"Altima", "Sentra", "Rogue", "Frontier", "Pathfinder"});
        } else if (make == "Dodge") {
            m_model_combo->addItems({"Ram 1500", "Charger", "Challenger", "Durango", "Grand Caravan"});
        } else if (make == "Jeep") {
            m_model_combo->addItems({"Grand Cherokee", "Wrangler", "Cherokee", "Compass", "Renegade"});
        } else if (make == "GMC") {
            m_model_combo->addItems({"Sierra 1500", "Yukon", "Acadia", "Terrain", "Canyon"});
        } else if (make == "Subaru") {
            m_model_combo->addItems({"Outback", "Forester", "Impreza", "Crosstrek", "Legacy"});
        } else if (make == "Hyundai") {
            m_model_combo->addItems({"Elantra", "Sonata", "Tucson", "Santa Fe", "Kona"});
        }
    } else {
        for (const auto& model : db_models) {
            m_model_combo->addItem(QString::fromStdString(model));
        }
    }
}

void AddVehicleDialog::onModelChanged(const QString& model) {
    if (model.isEmpty()) return;
    std::string specs = m_db->getCarEngineSpecs(m_make_combo->currentText().toStdString(), model.toStdString());
    if (!specs.empty()) {
        m_engine_edit->setText(QString::fromStdString(specs));
    }
}

int AddVehicleDialog::year() const {
    return m_year_combo->currentText().toInt();
}

std::string AddVehicleDialog::make() const {
    return m_make_combo->currentText().toStdString();
}

std::string AddVehicleDialog::model() const {
    return m_model_combo->currentText().toStdString();
}

std::string AddVehicleDialog::licensePlate() const {
    return m_plate_edit->text().trimmed().toStdString();
}

std::string AddVehicleDialog::engineSpecs() const {
    return m_engine_edit->text().trimmed().toStdString();
}

void AddVehicleDialog::onSave() {
    if (licensePlate().empty()) {
        QMessageBox::warning(this, "Empty Field", "License Plate is required.");
        return;
    }
    accept();
}

// ==========================================
// SIGNATURE PAD DIALOG
// ==========================================
SignaturePadDialog::SignaturePadDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Estimate Approval Signature");
    resize(400, 300);
    setMinimumSize(400, 300);

    auto main_layout = new QVBoxLayout(this);
    auto instructions = new QLabel("Please sign inside the box below:", this);
    instructions->setStyleSheet("font-weight: bold;");
    main_layout->addWidget(instructions);

    auto btn_layout = new QHBoxLayout();
    m_clear_btn = new QPushButton("Clear", this);
    m_clear_btn->setStyleSheet("padding: 6px 12px;");
    m_ok_btn = new QPushButton("Accept Signature", this);
    m_ok_btn->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 12px;");

    btn_layout->addStretch();
    btn_layout->addWidget(m_clear_btn);
    btn_layout->addWidget(m_ok_btn);

    main_layout->addStretch();
    main_layout->addLayout(btn_layout);

    connect(m_clear_btn, &QPushButton::clicked, this, &SignaturePadDialog::onClear);
    connect(m_ok_btn, &QPushButton::clicked, this, &SignaturePadDialog::onAccept);
}

void SignaturePadDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QList<QPoint> new_stroke;
        new_stroke.append(event->pos());
        m_strokes.append(new_stroke);
        update();
    }
}

void SignaturePadDialog::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_strokes.isEmpty()) {
        m_strokes.last().append(event->pos());
        update();
    }
}

void SignaturePadDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw bounds for signature box
    painter.setPen(QPen(Qt::gray, 2, Qt::DashLine));
    painter.drawRect(10, 30, width() - 20, height() - 80);

    painter.setPen(QPen(Qt::blue, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& stroke : m_strokes) {
        for (int i = 0; i < stroke.size() - 1; ++i) {
            painter.drawLine(stroke[i], stroke[i+1]);
        }
    }
}

void SignaturePadDialog::onClear() {
    m_strokes.clear();
    m_sig_base64.clear();
    update();
}

void SignaturePadDialog::onAccept() {
    if (m_strokes.isEmpty()) {
        QMessageBox::warning(this, "Empty Signature", "Please provide a signature before accepting.");
        return;
    }

    // Render strokes to a QImage
    QImage image(width(), height(), QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& stroke : m_strokes) {
        for (int i = 0; i < stroke.size() - 1; ++i) {
            painter.drawLine(stroke[i], stroke[i+1]);
        }
    }
    painter.end();

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    m_sig_base64 = QString::fromLatin1(ba.toBase64());

    accept();
}

// ==========================================
// NEW INTAKE WIZARD DIALOG
// ==========================================
NewIntakeWizardDialog::NewIntakeWizardDialog(std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle("🆕 Customer & Vehicle Intake Wizard");
    resize(500, 420);

    auto main_layout = new QVBoxLayout(this);

    auto header_lbl = new QLabel("Enter Customer & Vehicle details for instant intake:", this);
    header_lbl->setStyleSheet("font-weight: bold; font-size: 13px; color: #1976d2;");
    main_layout->addWidget(header_lbl);

    auto cust_box = new QGroupBox("Customer Information", this);
    auto cust_form = new QFormLayout(cust_box);
    m_first_name_edit = new QLineEdit(this);
    m_last_name_edit = new QLineEdit(this);
    m_phone_edit = new QLineEdit(this);
    m_email_edit = new QLineEdit(this);

    cust_form->addRow("First Name *:", m_first_name_edit);
    cust_form->addRow("Last Name *:", m_last_name_edit);
    cust_form->addRow("Phone Number:", m_phone_edit);
    cust_form->addRow("Email Address:", m_email_edit);
    main_layout->addWidget(cust_box);

    auto veh_box = new QGroupBox("Vehicle Information", this);
    auto veh_form = new QFormLayout(veh_box);
    m_plate_edit = new QLineEdit(this);
    m_year_edit = new QLineEdit(this);
    m_model_edit = new QLineEdit(this);
    m_engine_edit = new QLineEdit(this);
    m_mileage_edit = new QLineEdit(this);

    m_year_edit->setText("2022");

    veh_form->addRow("License Plate *:", m_plate_edit);
    veh_form->addRow("Year:", m_year_edit);
    veh_form->addRow("Model / Trim:", m_model_edit);
    veh_form->addRow("Engine Specs:", m_engine_edit);
    veh_form->addRow("Odometer In:", m_mileage_edit);
    main_layout->addWidget(veh_box);

    auto btn_box = new QHBoxLayout();
    auto cancel_btn = new QPushButton("Cancel", this);
    auto complete_btn = new QPushButton("🚀 Create Work Order", this);
    complete_btn->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 8px 16px;");

    btn_box->addStretch();
    btn_box->addWidget(cancel_btn);
    btn_box->addWidget(complete_btn);
    main_layout->addLayout(btn_box);

    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(complete_btn, &QPushButton::clicked, this, &NewIntakeWizardDialog::onCompleteIntake);
}

void NewIntakeWizardDialog::onCompleteIntake() {
    std::string first = m_first_name_edit->text().trimmed().toStdString();
    std::string last = m_last_name_edit->text().trimmed().toStdString();
    std::string plate = m_plate_edit->text().trimmed().toStdString();

    if (first.empty() || last.empty() || plate.empty()) {
        QMessageBox::warning(this, "Missing Fields", "Please provide First Name, Last Name, and License Plate to complete intake.");
        return;
    }

    Customer c;
    c.first_name = first;
    c.last_name = last;
    c.phone_number = m_phone_edit->text().trimmed().toStdString();
    c.email = m_email_edit->text().trimmed().toStdString();

    m_created_customer_id = m_db->insertCustomer(c);
    if (m_created_customer_id == -1) {
        QMessageBox::critical(this, "Database Error", "Failed to save customer to database.");
        return;
    }

    Vehicle v;
    v.customer_id = m_created_customer_id;
    v.license_plate = plate;
    v.year = m_year_edit->text().toInt();
    v.model = m_model_edit->text().trimmed().toStdString();
    v.engine_specs = m_engine_edit->text().trimmed().toStdString();

    m_created_vehicle_id = m_db->insertVehicle(v);
    if (m_created_vehicle_id == -1) {
        QMessageBox::critical(this, "Database Error", "Failed to save vehicle to database.");
        return;
    }

    accept();
}

} // namespace tuxrepair
