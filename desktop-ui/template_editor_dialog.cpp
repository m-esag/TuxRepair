#include "template_editor_dialog.h"
#include "traveler_renderer.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QSplitter>

namespace tuxrepair {

TemplateEditorDialog::TemplateEditorDialog(std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle("Setup Print Templates");
    resize(1000, 600);

    auto main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    auto splitter = new QSplitter(Qt::Horizontal, this);
    main_layout->addWidget(splitter);

    // Left column: Template management
    auto left_widget = new QWidget(this);
    auto left_layout = new QVBoxLayout(left_widget);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(8);

    left_layout->addWidget(new QLabel("Print Templates:", this));
    m_list_widget = new QListWidget(this);
    left_layout->addWidget(m_list_widget);

    m_set_active_btn = new QPushButton("⭐ Set As Active Template", this);
    m_set_active_btn->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold;");
    left_layout->addWidget(m_set_active_btn);

    auto list_btn_layout = new QHBoxLayout();
    m_add_btn = new QPushButton("Add New", this);
    m_delete_btn = new QPushButton("Delete Selected", this);
    list_btn_layout->addWidget(m_add_btn);
    list_btn_layout->addWidget(m_delete_btn);
    left_layout->addLayout(list_btn_layout);

    splitter->addWidget(left_widget);

    // Middle column: Code Editor
    auto mid_widget = new QWidget(this);
    auto mid_layout = new QVBoxLayout(mid_widget);
    mid_layout->setContentsMargins(0, 0, 0, 0);
    mid_layout->setSpacing(8);

    mid_layout->addWidget(new QLabel("Edit Template HTML/CSS Code:", this));
    m_code_edit = new QTextEdit(this);
    m_code_edit->setFontPointSize(10);
    m_code_edit->setFontFamily("Courier New");
    mid_layout->addWidget(m_code_edit);

    // Placeholder Quick Buttons
    auto placeholders_lay = new QHBoxLayout();
    auto addPlaceholderBtn = [this, placeholders_lay](const QString& name, const QString& tag) {
        auto btn = new QPushButton(name, this);
        btn->setStyleSheet("font-size: 11px; padding: 2px 4px;");
        connect(btn, &QPushButton::clicked, this, [this, tag](){ onInsertPlaceholder(tag); });
        placeholders_lay->addWidget(btn);
    };
    addPlaceholderBtn("Plate", "{{VEHICLE_PLATE}}");
    addPlaceholderBtn("Cust Name", "{{CUSTOMER_NAME}}");
    addPlaceholderBtn("Items Table", "{{TICKET_ITEMS_TABLE}}");
    addPlaceholderBtn("Grand Total", "{{TICKET_GRAND_TOTAL}}");
    addPlaceholderBtn("Barcode", "{{BARCODE_SVG}}");
    mid_layout->addLayout(placeholders_lay);

    splitter->addWidget(mid_widget);

    // Right column: Live preview
    auto right_widget = new QWidget(this);
    auto right_layout = new QVBoxLayout(right_widget);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(8);

    right_layout->addWidget(new QLabel("Live Preview (Mock Ticket):", this));
    m_preview_browser = new QTextBrowser(this);
    right_layout->addWidget(m_preview_browser);

    splitter->addWidget(right_widget);

    // Set widths
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 2);

    // Connects
    connect(m_list_widget, &QListWidget::currentRowChanged, this, &TemplateEditorDialog::onTemplateSelected);
    connect(m_code_edit, &QTextEdit::textChanged, this, &TemplateEditorDialog::onCodeChanged);
    connect(m_add_btn, &QPushButton::clicked, this, &TemplateEditorDialog::onAddTemplate);
    connect(m_delete_btn, &QPushButton::clicked, this, &TemplateEditorDialog::onDeleteTemplate);
    connect(m_set_active_btn, &QPushButton::clicked, this, &TemplateEditorDialog::onSetActiveTemplate);

    refreshList();
}

void TemplateEditorDialog::refreshList() {
    m_templates = m_db->getPrintTemplates();
    m_list_widget->clear();
    
    int active_idx = 0;
    for (size_t i = 0; i < m_templates.size(); ++i) {
        QString text = QString::fromStdString(m_templates[i].name);
        if (m_templates[i].is_active) {
            text += " (Active ⭐)";
            active_idx = static_cast<int>(i);
        }
        m_list_widget->addItem(text);
    }

    if (!m_templates.empty()) {
        m_list_widget->setCurrentRow(active_idx);
    }
}

void TemplateEditorDialog::onTemplateSelected(int index) {
    if (index < 0 || index >= static_cast<int>(m_templates.size())) {
        m_current_index = -1;
        m_code_edit->clear();
        m_preview_browser->clear();
        return;
    }

    m_current_index = index;
    loadCurrentTemplate();
}

void TemplateEditorDialog::loadCurrentTemplate() {
    if (m_current_index == -1) return;
    
    m_updating_code = true;
    m_code_edit->setPlainText(QString::fromStdString(m_templates[m_current_index].content_html));
    m_updating_code = false;
    
    updatePreview();
}

void TemplateEditorDialog::onCodeChanged() {
    if (m_updating_code || m_current_index == -1) return;

    // Save locally
    m_templates[m_current_index].content_html = m_code_edit->toPlainText().toStdString();
    
    // Save to DB
    m_db->updatePrintTemplate(m_templates[m_current_index]);

    updatePreview();
}

void TemplateEditorDialog::onInsertPlaceholder(const QString& placeholder) {
    m_code_edit->insertPlainText(placeholder);
}

void TemplateEditorDialog::onAddTemplate() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Create Template", "Enter new template name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    PrintTemplate tmpl;
    tmpl.name = name.trimmed().toStdString();
    tmpl.content_html = "<html><body><h1>" + tmpl.name + "</h1>{{TICKET_ITEMS_TABLE}}</body></html>";
    tmpl.is_active = 0;

    if (m_db->addPrintTemplate(tmpl)) {
        refreshList();
    } else {
        QMessageBox::critical(this, "Error", "Failed to add template. Name must be unique.");
    }
}

void TemplateEditorDialog::onDeleteTemplate() {
    if (m_current_index == -1) return;
    
    auto tmpl = m_templates[m_current_index];
    if (tmpl.is_active) {
        QMessageBox::warning(this, "Delete Blocked", "Cannot delete active template. Set another template as active first.");
        return;
    }

    auto res = QMessageBox::question(this, "Delete", QString("Are you sure you want to delete template: %1?").arg(QString::fromStdString(tmpl.name)));
    if (res == QMessageBox::Yes) {
        m_db->deletePrintTemplate(tmpl.id);
        refreshList();
    }
}

void TemplateEditorDialog::onSetActiveTemplate() {
    if (m_current_index == -1) return;
    
    m_db->setActivePrintTemplate(m_templates[m_current_index].id);
    refreshList();
}

void TemplateEditorDialog::updatePreview() {
    if (m_current_index == -1) return;

    Invoice mock = getMockInvoice();
    std::string html = TravelerRenderer::generateTravelerHTML(mock, m_templates[m_current_index].content_html);
    m_preview_browser->setHtml(QString::fromStdString(html));
}

Invoice TemplateEditorDialog::getMockInvoice() {
    Invoice inv;
    inv.id = 1205;
    inv.ticket_type = "Estimate";
    inv.date_created = "2026-07-11";
    inv.mileage_in = 110240;
    
    inv.customer.first_name = "Jane";
    inv.customer.last_name = "Doe";
    inv.customer.phone_number = "(555) 019-9238";
    inv.customer.address = "1283 Maple St";
    inv.customer.city = "Tulare, CA";

    inv.vehicle.license_plate = "7XYZ89";
    inv.vehicle.year = 2018;
    inv.vehicle.model = "Toyota Camry";
    inv.vehicle.engine_specs = "2.5L I4 DOHC";
    inv.vehicle.vin = "1NXBU4EE9JZ19028";

    InvoiceItem item1;
    item1.part_number = "5W30-SB";
    item1.description = "5W-30 Synthetic Blend Oil (Quart)";
    item1.quantity = 5.0;
    item1.unit_price = 500; // $5.00
    item1.specification = "Part";
    inv.items.push_back(item1);

    InvoiceItem item2;
    item2.part_number = "OF-PREM";
    item2.description = "Premium Spin-on Oil Filter";
    item2.quantity = 1.0;
    item2.unit_price = 800; // $8.00
    item2.specification = "Part";
    inv.items.push_back(item2);

    InvoiceItem item3;
    item3.part_number = "LOB-OIL";
    item3.description = "Standard Lube, Oil & Filter Labor";
    item3.quantity = 1.0;
    item3.unit_price = 2500; // $25.00
    item3.specification = "Labor";
    inv.items.push_back(item3);

    return inv;
}

} // namespace tuxrepair
