#include "qol_dialogs.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>

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
    m_field_combo->addItems({"Last Name", "First Name", "Phone"});
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

    connect(search_btn, &QPushButton::clicked, this, &CustomerLookupDialog::onSearch);
    connect(m_select_btn, &QPushButton::clicked, this, &CustomerLookupDialog::onSelect);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_results_table, &QTableWidget::cellDoubleClicked, this, [this](int, int){ onSelect(); });

    // Initial load
    onSearch();
}

void CustomerLookupDialog::onSearch() {
    std::string text = m_search_edit->text().trimmed().toStdString();
    std::string field = "last_name";
    if (m_field_combo->currentIndex() == 1) field = "first_name";
    else if (m_field_combo->currentIndex() == 2) field = "phone_number";

    auto customers = m_db->searchCustomers(field, text);
    m_results_table->setRowCount(0);

    for (const auto& c : customers) {
        // Query vehicles for this customer
        auto vehicles = m_db->searchVehiclesByPlate("%");
        for (const auto& v : vehicles) {
            if (v.customer_id != c.id) continue;

            int row = m_results_table->rowCount();
            m_results_table->insertRow(row);

            auto name_item = new QTableWidgetItem(QString("%1, %2").arg(QString::fromStdString(c.last_name)).arg(QString::fromStdString(c.first_name)));
            // Save IDs
            name_item->setData(Qt::UserRole, c.id);
            name_item->setData(Qt::UserRole + 1, v.id);

            m_results_table->setItem(row, 0, name_item);
            m_results_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(c.phone_number)));
            m_results_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(v.license_plate)));
            m_results_table->setItem(row, 3, new QTableWidgetItem(QString("%1 %2").arg(v.year).arg(QString::fromStdString(v.model))));
            m_results_table->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(v.engine_specs)));
        }
    }
}

void CustomerLookupDialog::onSelect() {
    auto ranges = m_results_table->selectedRanges();
    if (ranges.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select a customer row from the results table.");
        return;
    }

    int row = ranges.first().topRow();
    auto name_item = m_results_table->item(row, 0);
    int cust_id = name_item->data(Qt::UserRole).toInt();
    int veh_id = name_item->data(Qt::UserRole + 1).toInt();

    m_db->getCustomer(cust_id, m_selected_customer);
    m_db->getVehicle(veh_id, m_selected_vehicle);
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
    m_catalog_table->setColumnCount(4);
    m_catalog_table->setHorizontalHeaderLabels({"Type", "SKU / Code", "Description", "Unit Retail Price"});
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
            
            double d_price = item.retail_price / 100.0;
            m_catalog_table->setItem(row, 3, new QTableWidgetItem(QString::number(d_price, 'f', 2)));
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
            m_catalog_table->setItem(row, 3, new QTableWidgetItem(QString::number(l.price, 'f', 2)));
        }
    }
}

void CatalogLookupDialog::onSelect() {
    auto ranges = m_catalog_table->selectedRanges();
    if (ranges.isEmpty()) return;

    int row = ranges.first().topRow();
    m_selected_type = m_catalog_table->item(row, 0)->text();
    m_selected_code = m_catalog_table->item(row, 1)->text();
    m_selected_desc = m_catalog_table->item(row, 2)->text();
    m_selected_price = m_catalog_table->item(row, 3)->text().toDouble();
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

} // namespace tuxrepair
