#include "main_window.h"
#include "qol_dialogs.h"
#include "register_dialog.h"
#include "record_payment_dialog.h"
#include "template_editor_dialog.h"
#include "traveler_renderer.h"
#include <QCompleter>
#include <QStringListModel>
#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>
#include <algorithm>
#include <cctype>
#include <map>

namespace tuxrepair {

MainWindow::MainWindow(std::shared_ptr<DBManager> db, QWidget *parent)
    : QMainWindow(parent), m_db(db) {
  setWindowTitle("TuxRepair - Service Counter & Double-Entry Ledger");
  setWindowIcon(QIcon(":/app_icon.png"));
  resize(1024, 768);

  applyTheme();

  m_tab_widget = new QTabWidget(this);
  setCentralWidget(m_tab_widget);

  m_tickets_tab = new QWidget(this);
  m_scheduler_tab = new QWidget(this);
  m_inventory_tab = new QWidget(this);
  m_ledger_tab = new QWidget(this);

  m_tab_widget->addTab(m_tickets_tab, "Work Orders & Counter");
  m_tab_widget->addTab(m_scheduler_tab, "Bay Scheduler");
  m_tab_widget->addTab(m_inventory_tab, "Inventory Manager");
  m_tab_widget->addTab(m_ledger_tab, "Accounting Ledger");

  setupTicketsTab();
  setupSchedulerTab();
  setupInventoryTab();
  setupLedgerTab();
  setupMenuBar();

  // Load persisted settings
  std::string tax_rate_str = m_db->getSetting("sales_tax_rate", "0.08");
  std::string supplies_pct_str = m_db->getSetting("supplies_percent", "0.10");
  std::string supplies_cap_str = m_db->getSetting("supplies_cap_cents", "3500");
  try {
    m_sales_tax_rate = std::stod(tax_rate_str);
    m_supplies_percent = std::stod(supplies_pct_str);
    m_supplies_cap_cents = std::stoll(supplies_cap_str);
  } catch (...) {
    m_sales_tax_rate = 0.08;
    m_supplies_percent = 0.10;
    m_supplies_cap_cents = 3500;
  }

  // Default to search input having focus
  m_intake_lookup_edit->setFocus();
}

MainWindow::~MainWindow() {}

void MainWindow::setupTicketsTab() {
  auto container_layout = new QVBoxLayout(m_tickets_tab);
  container_layout->setContentsMargins(4, 4, 4, 4);
  container_layout->setSpacing(4);

  // =========================================================================
  // 1. TOP PIPELINE BAR (Clickable Segmented Statuses)
  // =========================================================================
  m_status_pipeline_widget = new QWidget(this);
  auto pipeline_layout = new QHBoxLayout(m_status_pipeline_widget);
  pipeline_layout->setContentsMargins(0, 0, 0, 0);
  pipeline_layout->setSpacing(2);

  QStringList stages = {"New", "Intake", "Estimate", "Awaiting Approval", "Approved", 
                        "In Progress", "Waiting on Parts", "Ready", "Invoiced", "Closed"};
  
  for (int i = 0; i < stages.size(); ++i) {
    auto btn = new QPushButton(stages[i], this);
    btn->setCheckable(true);
    btn->setStyleSheet("padding: 6px; font-weight: bold; border: 1px solid #ccc; background-color: #f5f5f5;");
    pipeline_layout->addWidget(btn);
    m_status_buttons.push_back(btn);

    connect(btn, &QPushButton::clicked, this, [this, i]() {
      onStatusButtonClicked(i);
    });
  }
  container_layout->addWidget(m_status_pipeline_widget);

  // =========================================================================
  // 2. ACTION TOOLBAR
  // =========================================================================
  m_action_toolbar_widget = new QWidget(this);
  auto toolbar_layout = new QHBoxLayout(m_action_toolbar_widget);
  toolbar_layout->setContentsMargins(0, 2, 0, 2);
  toolbar_layout->setSpacing(4);

  m_btn_new_ro = new QPushButton("🆕 New RO", this);
  m_btn_new_ro->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 12px;");
  
  m_btn_save_ro = new QPushButton("💾 Save", this);
  m_btn_save_ro->setStyleSheet("background-color: #1976d2; color: white; font-weight: bold; padding: 6px 12px;");

  m_btn_print_ro = new QPushButton("🖨️ Print Output", this);
  m_btn_print_ro->setStyleSheet("padding: 6px 12px;");

  m_btn_send_est = new QPushButton("✉️ Send Estimate", this);
  m_btn_send_est->setStyleSheet("padding: 6px 12px;");

  m_btn_approve_est = new QPushButton("✍️ Approve", this);
  m_btn_approve_est->setStyleSheet("background-color: #00897b; color: white; font-weight: bold; padding: 6px 12px;");

  m_btn_convert_inv = new QPushButton("Convert to Invoice", this);
  m_btn_convert_inv->setStyleSheet("padding: 6px 12px;");

  m_btn_record_payment = new QPushButton("💳 Record Payment", this);
  m_btn_record_payment->setStyleSheet(
      "background-color: #1565c0; color: white; font-weight: bold; padding: 6px 12px;");

  m_btn_mark_ready = new QPushButton("Mark Ready", this);
  m_btn_mark_ready->setStyleSheet("padding: 6px 12px;");

  m_btn_close_ro = new QPushButton("Close RO", this);
  m_btn_close_ro->setStyleSheet("padding: 6px 12px;");

  m_btn_duplicate = new QPushButton("Duplicate", this);
  m_btn_duplicate->setStyleSheet("padding: 6px 12px;");

  m_btn_void_reopen = new QPushButton("Void/Reopen", this);
  m_btn_void_reopen->setStyleSheet("padding: 6px 12px;");

  // m_finalize_ticket_btn carries the F12-activated finalize menu. Its action
  // is wired later in setupTicketsTab() once onFinalizeInvoice() is reachable.
  m_finalize_ticket_btn = new QPushButton("⚡ Finalize ▾", this);
  m_finalize_ticket_btn->setStyleSheet(
      "background-color: #c62828; color: white; font-weight: bold; padding: 6px 12px;");
  {
    auto* finalize_menu = new QMenu(m_finalize_ticket_btn);
    finalize_menu->addAction("Finalize / Post to Ledger", this,
                             [this]() { onFinalizeInvoice(); });
    m_finalize_ticket_btn->setMenu(finalize_menu);
  }

  toolbar_layout->addWidget(m_btn_new_ro);
  toolbar_layout->addWidget(m_btn_save_ro);
  toolbar_layout->addWidget(m_btn_print_ro);
  toolbar_layout->addWidget(m_btn_send_est);
  toolbar_layout->addWidget(m_btn_approve_est);
  toolbar_layout->addWidget(m_btn_convert_inv);
  toolbar_layout->addWidget(m_btn_record_payment);
  toolbar_layout->addWidget(m_btn_mark_ready);
  toolbar_layout->addWidget(m_btn_close_ro);
  toolbar_layout->addWidget(m_btn_duplicate);
  toolbar_layout->addWidget(m_btn_void_reopen);
  toolbar_layout->addWidget(m_finalize_ticket_btn);
  toolbar_layout->addStretch();

  container_layout->addWidget(m_action_toolbar_widget);

  // =========================================================================
  // 3. MAIN SPLITTER (Left, Center, Right Panels)
  // =========================================================================
  m_main_splitter = new QSplitter(Qt::Horizontal, this);
  container_layout->addWidget(m_main_splitter);

  // --- LEFT PANEL: Search / Intake list ---
  auto left_widget = new QWidget(this);
  auto left_layout = new QVBoxLayout(left_widget);
  left_layout->setContentsMargins(0, 0, 0, 0);
  left_layout->setSpacing(4);

  // Intake / Search Box
  auto search_group = new QGroupBox("Powerful Intake lookup", this);
  auto search_layout = new QVBoxLayout(search_group);
  search_layout->setSpacing(4);

  auto lookup_row = new QHBoxLayout();
  m_intake_lookup_type_combo = new QComboBox(this);
  m_intake_lookup_type_combo->addItems({"License Plate", "VIN", "Phone", "Email", "Customer Name"});
  lookup_row->addWidget(m_intake_lookup_type_combo);

  m_intake_lookup_edit = new QLineEdit(this);
  m_intake_lookup_edit->setPlaceholderText("Search terms...");
  m_search_completer = new QCompleter(this);
  m_search_completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_search_completer->setFilterMode(Qt::MatchContains);
  m_intake_lookup_edit->setCompleter(m_search_completer);
  lookup_row->addWidget(m_intake_lookup_edit);

  m_intake_search_btn = new QPushButton("Find", this);
  lookup_row->addWidget(m_intake_search_btn);
  search_layout->addLayout(lookup_row);

  left_layout->addWidget(search_group);

  // m_tickets_left_tabs holds the two left-panel views:
  //   tab 0 = active repair orders (m_invoices_table)
  //   tab 1 = live search results (m_intake_results_table)
  // onPlateSearchTextChanged flips to tab 1 while typing; onSelectIntakeVehicle
  // flips back to tab 0 once a vehicle is chosen.
  m_tickets_left_tabs = new QTabWidget(this);
  m_tickets_left_tabs->setDocumentMode(true);

  // Active Invoices Table
  m_invoices_table = new QTableWidget(this);
  m_invoices_table->setColumnCount(4);
  m_invoices_table->setHorizontalHeaderLabels({"ID", "Customer", "Plate / Name", "Status"});
  m_invoices_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_invoices_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_invoices_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tickets_left_tabs->addTab(m_invoices_table, "Active ROs");

  // Live search results table — populated by onPlateSearchTextChanged.
  m_intake_results_table = new QTableWidget(this);
  m_intake_results_table->setColumnCount(4);
  m_intake_results_table->setHorizontalHeaderLabels(
      {"Plate", "Vehicle", "Customer", "Engine"});
  m_intake_results_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_intake_results_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_intake_results_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tickets_left_tabs->addTab(m_intake_results_table, "Search Results");

  left_layout->addWidget(m_tickets_left_tabs);

  m_main_splitter->addWidget(left_widget);

  // --- CENTER PANEL: Active Work Order Details & Grid ---
  auto center_widget = new QWidget(this);
  auto center_layout = new QVBoxLayout(center_widget);
  center_layout->setContentsMargins(0, 0, 0, 0);
  center_layout->setSpacing(4);

  // Header Summary / Info
  auto header_info_layout = new QHBoxLayout();
  m_ticket_title_label = new QLabel("Select or create a work order to begin.", this);
  m_ticket_title_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #1976d2;");
  header_info_layout->addWidget(m_ticket_title_label);

  m_ticket_status_label = new QLabel("", this);
  m_ticket_status_label->setStyleSheet("font-weight: bold; font-size: 13px; color: #2e7d32;");
  header_info_layout->addWidget(m_ticket_status_label);
  
  m_nav_invoice_spin = new QSpinBox(this);
  m_nav_invoice_spin->setRange(1, 99999);
  m_nav_invoice_spin->setPrefix("RO #");
  header_info_layout->addWidget(m_nav_invoice_spin);
  center_layout->addLayout(header_info_layout);

  // Customer & Vehicle fields
  auto forms_layout = new QHBoxLayout();
  
  auto cust_group = new QGroupBox("Customer Context", this);
  auto cust_form = new QFormLayout(cust_group);
  m_t_cust_first_edit = new QLineEdit(this);
  m_t_cust_last_edit = new QLineEdit(this);
  m_t_cust_phone_edit = new QLineEdit(this);
  cust_form->addRow("First Name:", m_t_cust_first_edit);
  cust_form->addRow("Last Name:", m_t_cust_last_edit);
  cust_form->addRow("Phone Number:", m_t_cust_phone_edit);
  forms_layout->addWidget(cust_group);

  auto veh_group = new QGroupBox("Vehicle Metadata", this);
  auto veh_form = new QFormLayout(veh_group);
  m_t_veh_plate_edit = new QLineEdit(this);
  m_t_veh_model_edit = new QLineEdit(this);
  m_t_veh_engine_edit = new QLineEdit(this);
  m_ticket_mileage_in_edit = new QLineEdit(this);
  m_ticket_mileage_out_edit = new QLineEdit(this);

  veh_form->addRow("License Plate:", m_t_veh_plate_edit);
  veh_form->addRow("Model Info:", m_t_veh_model_edit);
  veh_form->addRow("Engine specs:", m_t_veh_engine_edit);
  veh_form->addRow("Odometer In:", m_ticket_mileage_in_edit);
  veh_form->addRow("Odometer Out:", m_ticket_mileage_out_edit);
  forms_layout->addWidget(veh_group);
  center_layout->addLayout(forms_layout);

  // Meta row (type + writer)
  auto meta_row = new QHBoxLayout();
  m_ticket_type_combo = new QComboBox(this);
  m_ticket_type_combo->addItems({"Quote", "Estimate", "Invoice"});
  meta_row->addWidget(new QLabel("Type:", this));
  meta_row->addWidget(m_ticket_type_combo);

  m_billed_by_edit = new QLineEdit(this);
  meta_row->addWidget(new QLabel("Billed By / Writer:", this));
  meta_row->addWidget(m_billed_by_edit);
  center_layout->addLayout(meta_row);

  // Line items grid (9-column upgraded workspace)
  m_items_table = new QTableWidget(this);
  m_items_table->setColumnCount(9);
  m_items_table->setHorizontalHeaderLabels({"Type", "Part # / Code", "Description", "Qty / Hours", "Rate", "Total", "Tech", "Tax", "Note"});
  m_items_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  m_items_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch); // Description stretches
  m_items_table->setContextMenuPolicy(Qt::CustomContextMenu);
  center_layout->addWidget(m_items_table);

  // Line actions buttons
  auto line_actions = new QHBoxLayout();
  m_add_part_btn = new QPushButton("+ Add Part", this);
  m_add_labor_btn = new QPushButton("+ Add Labor", this);
  m_remove_item_btn = new QPushButton("- Remove Item", this);
  m_insert_job_kit_btn = new QPushButton("🛠 Insert Job Kit...", this);
  m_insert_catalog_btn = new QPushButton("📦 Catalog Lookup...", this);
  m_move_up_btn = new QPushButton("▲", this);
  m_move_down_btn = new QPushButton("▼", this);

  line_actions->addWidget(m_add_part_btn);
  line_actions->addWidget(m_add_labor_btn);
  line_actions->addWidget(m_remove_item_btn);
  line_actions->addWidget(m_insert_job_kit_btn);
  line_actions->addWidget(m_insert_catalog_btn);
  line_actions->addWidget(m_move_up_btn);
  line_actions->addWidget(m_move_down_btn);
  center_layout->addLayout(line_actions);

  m_main_splitter->addWidget(center_widget);

  // =========================================================================
  // Connect toolbar actions
  // =========================================================================
  connect(m_btn_new_ro, &QPushButton::clicked, this, &MainWindow::onNewRO);
  connect(m_btn_save_ro, &QPushButton::clicked, this, &MainWindow::onSaveRO);
  connect(m_btn_print_ro, &QPushButton::clicked, this, &MainWindow::onPrintRO);
  connect(m_btn_send_est, &QPushButton::clicked, this, &MainWindow::onSendEstimate);
  connect(m_btn_approve_est, &QPushButton::clicked, this, &MainWindow::onApproveEstimate);
  connect(m_btn_convert_inv, &QPushButton::clicked, this, &MainWindow::onConvertInvoice);
  connect(m_btn_record_payment, &QPushButton::clicked, this, &MainWindow::onRecordPayment);
  connect(m_btn_mark_ready, &QPushButton::clicked, this, &MainWindow::onMarkReady);
  connect(m_btn_close_ro, &QPushButton::clicked, this, &MainWindow::onCloseRO);
  connect(m_btn_duplicate, &QPushButton::clicked, this, &MainWindow::onDuplicateRO);
  connect(m_btn_void_reopen, &QPushButton::clicked, this, &MainWindow::onVoidReopenRO);
  connect(m_intake_search_btn, &QPushButton::clicked, this, &MainWindow::onIntakeSearch);

  // Wire standard table signals
  connect(m_invoices_table, &QTableWidget::cellClicked, this, [this](int row, int) { onInvoiceSelected(row); });
  connect(m_add_part_btn, &QPushButton::clicked, this, &MainWindow::onAddPartItem);
  connect(m_add_labor_btn, &QPushButton::clicked, this, &MainWindow::onAddLaborItem);
  connect(m_remove_item_btn, &QPushButton::clicked, this, &MainWindow::onRemoveSelectedItems);
  connect(m_insert_job_kit_btn, &QPushButton::clicked, this, &MainWindow::onInsertJobKit);
  connect(m_insert_catalog_btn, &QPushButton::clicked, this, [this]() {
      if (m_active_invoice_id == -1) return;
      CatalogLookupDialog dlg(m_db, this);
      if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
          m_items_table->blockSignals(true);
          int row = m_items_table->rowCount();
          m_items_table->insertRow(row);
          
          auto type_combo = new QComboBox(m_items_table);
          type_combo->addItems({"Part", "Labor", "Fee", "Sublet", "Discount"});
          type_combo->setCurrentText(dlg.selectedType());
          m_items_table->setCellWidget(row, 0, type_combo);
          
          m_items_table->setItem(row, 1, new QTableWidgetItem(dlg.selectedCode()));
          m_items_table->setItem(row, 2, new QTableWidgetItem(dlg.selectedDescription()));
          m_items_table->setItem(row, 3, new QTableWidgetItem("1"));
          m_items_table->setItem(row, 4, new QTableWidgetItem(QString::number(dlg.selectedPrice(), 'f', 2)));
          m_items_table->setItem(row, 5, new QTableWidgetItem("0.00"));
          
          auto tech_combo = new QComboBox(m_items_table);
          tech_combo->addItems({"Office", "Bob (Tech)", "Jane (Tech)", "Al (Tech)"});
          m_items_table->setCellWidget(row, 6, tech_combo);
          
          auto tax_item = new QTableWidgetItem();
          tax_item->setCheckState(Qt::Checked);
          m_items_table->setItem(row, 7, tax_item);
          
          m_items_table->setItem(row, 8, new QTableWidgetItem(""));
          
          m_items_table->blockSignals(false);
          recalculateTicketTotals();
      }
  });

  connect(m_move_up_btn, &QPushButton::clicked, this, &MainWindow::onMoveItemUp);
  connect(m_move_down_btn, &QPushButton::clicked, this, &MainWindow::onMoveItemDown);
  connect(m_items_table, &QTableWidget::cellChanged, this, &MainWindow::onCellChangedGrid);
  connect(m_items_table, &QTableWidget::customContextMenuRequested, this, &MainWindow::onItemsTableContextMenu);
  connect(m_items_table, &QTableWidget::cellClicked, this, &MainWindow::onItemsTableCellClicked);
  connect(m_nav_invoice_spin, &QSpinBox::valueChanged, this, [this](int val) {
      loadInvoiceDetails(val);
  });
  // 4. RIGHT SIDEBAR (Collapsible metadata, history, notes, signatures, attachments)
  // =========================================================================
  m_right_sidebar = new QWidget(this);
  auto right_sidebar_layout = new QVBoxLayout(m_right_sidebar);
  right_sidebar_layout->setContentsMargins(0, 0, 0, 0);
  right_sidebar_layout->setSpacing(4);

  m_right_tabs = new QTabWidget(this);
  right_sidebar_layout->addWidget(m_right_tabs);

  // Tab A: Service History Timeline
  auto history_tab_widget = new QWidget(this);
  auto history_tab_lay = new QVBoxLayout(history_tab_widget);
  m_history_list = new QTableWidget(this);
  m_history_list->setColumnCount(3);
  m_history_list->setHorizontalHeaderLabels({"Date", "Type", "Total"});
  m_history_list->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_history_list->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_history_list->setSelectionMode(QAbstractItemView::SingleSelection);
  history_tab_lay->addWidget(m_history_list);
  m_right_tabs->addTab(history_tab_widget, "Timeline");

  // Tab B: Notes Categories
  auto notes_tab_widget = new QWidget(this);
  auto notes_tab_lay = new QVBoxLayout(notes_tab_widget);
  auto notes_sub_tabs = new QTabWidget(this);

  m_notes_internal = new QTextEdit(this);
  m_notes_customer = new QTextEdit(this);
  m_notes_tech = new QTextEdit(this);
  m_notes_vehicle = new QTextEdit(this);
  m_notes_auth = new QTextEdit(this);

  notes_sub_tabs->addTab(m_notes_internal, "Internal");
  notes_sub_tabs->addTab(m_notes_customer, "Customer");
  notes_sub_tabs->addTab(m_notes_tech, "Tech");
  notes_sub_tabs->addTab(m_notes_vehicle, "Vehicle");
  notes_sub_tabs->addTab(m_notes_auth, "Authorization");
  notes_tab_lay->addWidget(notes_sub_tabs);
  m_right_tabs->addTab(notes_tab_widget, "Notes");

  // Tab C: Estimate Signature Approval
  auto sig_tab_widget = new QWidget(this);
  auto sig_tab_lay = new QVBoxLayout(sig_tab_widget);
  
  m_sig_preview_box = new QLabel("No signature capture found.", this);
  m_sig_preview_box->setAlignment(Qt::AlignCenter);
  m_sig_preview_box->setStyleSheet("border: 1px dashed gray; min-height: 120px; font-style: italic;");
  sig_tab_lay->addWidget(m_sig_preview_box);

  m_sig_status_lbl = new QLabel("Authorization status: Unsigned", this);
  m_sig_status_lbl->setStyleSheet("font-weight: bold;");
  sig_tab_lay->addWidget(m_sig_status_lbl);

  auto btn_draw_sig = new QPushButton("🖋️ Capture Signature...", this);
  btn_draw_sig->setStyleSheet("padding: 8px; font-weight: bold; background-color: #2e7d32; color: white;");
  sig_tab_lay->addWidget(btn_draw_sig);
  m_right_tabs->addTab(sig_tab_widget, "Signature");

  // Tab D: Attachments
  auto attach_tab_widget = new QWidget(this);
  auto attach_tab_lay = new QVBoxLayout(attach_tab_widget);
  
  m_attachments_table = new QTableWidget(this);
  m_attachments_table->setColumnCount(3);
  m_attachments_table->setHorizontalHeaderLabels({"File Name", "Uploaded At", "Type"});
  m_attachments_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_attachments_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_attachments_table->setSelectionMode(QAbstractItemView::SingleSelection);
  attach_tab_lay->addWidget(m_attachments_table);

  auto attach_btns = new QHBoxLayout();
  m_add_attachment_btn = new QPushButton("+ Add File", this);
  m_delete_attachment_btn = new QPushButton("- Remove File", this);
  attach_btns->addWidget(m_add_attachment_btn);
  attach_btns->addWidget(m_delete_attachment_btn);
  attach_tab_lay->addLayout(attach_btns);
  m_right_tabs->addTab(attach_tab_widget, "Attachments");

  m_main_splitter->addWidget(m_right_sidebar);

  // Set initial splitter stretches (Left = 20%, Center = 50%, Right = 30%)
  m_main_splitter->setStretchFactor(0, 2);
  m_main_splitter->setStretchFactor(1, 5);
  m_main_splitter->setStretchFactor(2, 3);

  // =========================================================================
  // 5. STICKY TOTALS SUMMARY AREA (At bottom of center pane)
  // =========================================================================
  auto summary_group = new QGroupBox("Work Order Financial Summary", this);
  auto summary_layout = new QGridLayout(summary_group);
  summary_layout->setSpacing(4);

  m_summary_parts_lbl = new QLabel("$0.00", this);
  m_summary_labor_lbl = new QLabel("$0.00", this);
  m_summary_supplies_lbl = new QLabel("$0.00", this);
  m_summary_subtotal_lbl = new QLabel("$0.00", this);
  m_summary_tax_lbl = new QLabel("$0.00", this);
  // m_summary_tax_title_lbl is the dynamic title for the tax row; its text is
  // refreshed in recalculateTicketTotals() to reflect the configured rate.
  m_summary_tax_title_lbl = new QLabel("Sales Tax:", this);
  m_summary_prepaid_lbl = new QLabel("$0.00", this);
  m_summary_total_lbl = new QLabel("$0.00", this);
  m_summary_balance_lbl = new QLabel("$0.00", this);

  m_summary_parts_lbl->setStyleSheet("font-weight: bold;");
  m_summary_labor_lbl->setStyleSheet("font-weight: bold;");
  m_summary_supplies_lbl->setStyleSheet("font-weight: bold;");
  m_summary_subtotal_lbl->setStyleSheet("font-weight: bold;");
  m_summary_tax_lbl->setStyleSheet("font-weight: bold;");
  m_summary_tax_title_lbl->setStyleSheet("font-weight: bold;");
  m_summary_prepaid_lbl->setStyleSheet("font-weight: bold; color: blue;");
  m_summary_total_lbl->setStyleSheet("font-weight: bold; font-size: 14px; color: #2e7d32;");
  m_summary_balance_lbl->setStyleSheet("font-weight: bold; font-size: 15px; color: red;");

  summary_layout->addWidget(new QLabel("Parts subtotal:", this), 0, 0);
  summary_layout->addWidget(m_summary_parts_lbl, 0, 1);
  summary_layout->addWidget(new QLabel("Labor subtotal:", this), 0, 2);
  summary_layout->addWidget(m_summary_labor_lbl, 0, 3);
  summary_layout->addWidget(new QLabel("Shop Supplies:", this), 0, 4);
  summary_layout->addWidget(m_summary_supplies_lbl, 0, 5);

  summary_layout->addWidget(m_summary_tax_title_lbl, 1, 0);
  summary_layout->addWidget(m_summary_tax_lbl, 1, 1);
  summary_layout->addWidget(new QLabel("Total Billed:", this), 1, 2);
  summary_layout->addWidget(m_summary_total_lbl, 1, 3);
  summary_layout->addWidget(new QLabel("Prepayments:", this), 1, 4);
  summary_layout->addWidget(m_summary_prepaid_lbl, 1, 5);

  auto bal_due_title = new QLabel("BALANCE DUE:", this);
  bal_due_title->setStyleSheet("font-weight: bold; font-size: 14px;");
  summary_layout->addWidget(bal_due_title, 2, 4);
  summary_layout->addWidget(m_summary_balance_lbl, 2, 5);

  center_layout->addWidget(summary_group);

  // Extra connects
  connect(btn_draw_sig, &QPushButton::clicked, this, &MainWindow::onApproveEstimate);
  connect(m_add_attachment_btn, &QPushButton::clicked, this, &MainWindow::onAddAttachment);
  connect(m_delete_attachment_btn, &QPushButton::clicked, this, &MainWindow::onDeleteAttachment);

  // Connect text area modifications for notes
  connect(m_notes_internal, &QTextEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_notes_customer, &QTextEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_notes_tech, &QTextEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_notes_vehicle, &QTextEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_notes_auth, &QTextEdit::textChanged, this, [this]() { markDirty(); });

  // Connect checkable metadata changes to mark dirty
  connect(m_t_cust_first_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_t_cust_last_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_t_cust_phone_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_t_veh_plate_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_t_veh_model_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_t_veh_engine_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_ticket_type_combo, &QComboBox::currentIndexChanged, this, [this](int) { markDirty(); });
  connect(m_ticket_mileage_in_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_ticket_mileage_out_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });
  connect(m_billed_by_edit, &QLineEdit::textChanged, this, [this]() { markDirty(); });

  refreshInvoicesList();
}

void MainWindow::setupLedgerTab() {
  auto main_layout = new QHBoxLayout(m_ledger_tab);
  auto splitter = new QSplitter(Qt::Horizontal, m_ledger_tab);
  main_layout->addWidget(splitter);

  // Left pane: Chart of Accounts
  auto left_widget = new QWidget(this);
  auto left_layout = new QVBoxLayout(left_widget);
  left_layout->addWidget(new QLabel("Chart of Accounts:", this));

  m_accounts_table = new QTableWidget(this);
  m_accounts_table->setColumnCount(3);
  m_accounts_table->setHorizontalHeaderLabels(
      {"Account Name", "Type", "Balance"});
  m_accounts_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  left_layout->addWidget(m_accounts_table);

  splitter->addWidget(left_widget);

  // Right pane: Transactions Ledger
  auto right_widget = new QWidget(this);
  auto right_layout = new QVBoxLayout(right_widget);
  right_layout->addWidget(
      new QLabel("Double-Entry Transactions History:", this));

  m_transactions_table = new QTableWidget(this);
  m_transactions_table->setColumnCount(4);
  m_transactions_table->setHorizontalHeaderLabels(
      {"Date", "Description", "Account Splits", "Amount"});
  m_transactions_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  right_layout->addWidget(m_transactions_table);

  m_export_ledger_btn = new QPushButton("Export Ledger to CSV...", this);
  m_export_ledger_btn->setStyleSheet("font-weight: bold; background-color: #2196f3; color: white; padding: 6px;");
  right_layout->addWidget(m_export_ledger_btn);

  splitter->addWidget(right_widget);

  connect(m_export_ledger_btn, &QPushButton::clicked, this, &MainWindow::onExportLedgerToCSV);

  connect(m_accounts_table, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) {
            auto item = m_accounts_table->item(row, 0);
            if (!item)
              return;
            int account_id = item->data(Qt::UserRole).toInt();
            QString name = item->text();
            RegisterDialog dlg(account_id, name, m_db, this);
            dlg.exec();
          });

  refreshAccountingData();
}

void MainWindow::onPlateSearchTextChanged(const QString &text) {
  if (text.isEmpty()) {
    m_intake_results_table->setRowCount(0);
    m_tickets_left_tabs->setCurrentIndex(0);
    return;
  }

  QString clean_text = text;
  int dash_idx = text.indexOf(" - ");
  if (dash_idx != -1) {
    clean_text = text.left(dash_idx).trimmed();
  }

  m_tickets_left_tabs->setCurrentIndex(1);
  auto vehicles = m_db->searchVehiclesByPlate(clean_text.toStdString());
  m_intake_results_table->setRowCount(0);

  for (const auto &v : vehicles) {
    int row = m_intake_results_table->rowCount();
    m_intake_results_table->insertRow(row);

    // Save IDs in cell data
    auto item_plate =
        new QTableWidgetItem(QString::fromStdString(v.license_plate));
    item_plate->setData(Qt::UserRole, v.id);
    item_plate->setData(Qt::UserRole + 1, v.customer_id);

    m_intake_results_table->setItem(row, 0, item_plate);
    m_intake_results_table->setItem(
        row, 1,
        new QTableWidgetItem(
            QString("%1 %2").arg(v.year).arg(QString::fromStdString(v.model))));

    Customer c;
    if (m_db->getCustomer(v.customer_id, c)) {
      m_intake_results_table->setItem(
          row, 2,
          new QTableWidgetItem(QString("%1, %2")
                                   .arg(QString::fromStdString(c.last_name))
                                   .arg(QString::fromStdString(c.first_name))));
    } else {
      m_intake_results_table->setItem(row, 2, new QTableWidgetItem("N/A"));
    }

    m_intake_results_table->setItem(
        row, 3, new QTableWidgetItem(QString::fromStdString(v.engine_specs)));
  }
}

void MainWindow::onPlateSearchReturnPressed() {
  if (m_intake_results_table->rowCount() > 0) {
    onSelectIntakeVehicle(0);
  }
}

void MainWindow::onSelectIntakeVehicle(int row) {
  auto item = m_intake_results_table->item(row, 0);
  if (!item)
    return;

  int vehicle_id = item->data(Qt::UserRole).toInt();
  int customer_id = item->data(Qt::UserRole + 1).toInt();

  auto invoices = m_db->getAllInvoices();
  int active_inv_id = -1;
  for (const auto &inv : invoices) {
    // "Active" = not yet posted to the ledger and not voided. Avoids creating
    // a duplicate ticket for a vehicle that already has a draft in progress.
    if (inv.vehicle_id == vehicle_id
        && inv.posted_tx_id == 0
        && inv.status != "Voided") {
      active_inv_id = inv.id;
      break;
    }
  }

  if (active_inv_id != -1) {
    loadInvoiceDetails(active_inv_id);
  } else {
    int inv_id = m_db->createInvoice(
        customer_id, vehicle_id, "Estimate", 0,
        QDateTime::currentDateTime().toString("yyyy-MM-dd").toStdString());
    if (inv_id != -1) {
      refreshInvoicesList();
      loadInvoiceDetails(inv_id);
    }
  }
  m_tickets_left_tabs->setCurrentIndex(0);
  m_intake_lookup_edit->clear();
}

void MainWindow::refreshInvoicesList() {
  auto list = m_db->getAllInvoices();
  m_invoices_table->setRowCount(0);

  for (const auto &inv : list) {
    int row = m_invoices_table->rowCount();
    m_invoices_table->insertRow(row);

    m_invoices_table->setItem(row, 0,
                              new QTableWidgetItem(QString::number(inv.id)));
    m_invoices_table->setItem(
        row, 1,
        new QTableWidgetItem(
            QString("%1, %2")
                .arg(QString::fromStdString(inv.customer.last_name))
                .arg(QString::fromStdString(inv.customer.first_name))));
    m_invoices_table->setItem(row, 2,
                              new QTableWidgetItem(QString::fromStdString(
                                  inv.vehicle.license_plate)));
    m_invoices_table->setItem(
        row, 3, new QTableWidgetItem(QString::fromStdString(inv.status)));
  }
  updateSearchCompleter();
}

void MainWindow::onMoveItemUp() {
  int r = m_items_table->currentRow();
  if (r <= 0) return;

  m_items_table->blockSignals(true);
  for (int col = 0; col < m_items_table->columnCount(); ++col) {
    QTableWidgetItem* item_r = m_items_table->takeItem(r, col);
    QTableWidgetItem* item_prev = m_items_table->takeItem(r - 1, col);
    m_items_table->setItem(r, col, item_prev);
    m_items_table->setItem(r - 1, col, item_r);
  }
  m_items_table->setCurrentCell(r - 1, m_items_table->currentColumn());
  m_items_table->blockSignals(false);
  markDirty();
}

void MainWindow::onMoveItemDown() {
  int r = m_items_table->currentRow();
  if (r < 0 || r >= m_items_table->rowCount() - 1) return;

  m_items_table->blockSignals(true);
  for (int col = 0; col < m_items_table->columnCount(); ++col) {
    QTableWidgetItem* item_r = m_items_table->takeItem(r, col);
    QTableWidgetItem* item_next = m_items_table->takeItem(r + 1, col);
    m_items_table->setItem(r, col, item_next);
    m_items_table->setItem(r + 1, col, item_r);
  }
  m_items_table->setCurrentCell(r + 1, m_items_table->currentColumn());
  m_items_table->blockSignals(false);
  markDirty();
}

void MainWindow::updateSearchCompleter() {
  QStringList list;
  auto invoices = m_db->getAllInvoices();
  std::set<std::string> unique_plates;
  for (const auto& inv : invoices) {
    if (!inv.vehicle.license_plate.empty() && unique_plates.find(inv.vehicle.license_plate) == unique_plates.end()) {
      unique_plates.insert(inv.vehicle.license_plate);
      // Format suggestion: "PLATE - Year Model (First Last)"
      QString item = QString("%1 - %2 %3 (%4 %5)")
          .arg(QString::fromStdString(inv.vehicle.license_plate))
          .arg(inv.vehicle.year)
          .arg(QString::fromStdString(inv.vehicle.model))
          .arg(QString::fromStdString(inv.customer.first_name))
          .arg(QString::fromStdString(inv.customer.last_name));
      list.append(item);
      list.append(QString::fromStdString(inv.vehicle.license_plate));
      list.append(QString::fromStdString(inv.customer.last_name));
    }
  }
  list.removeDuplicates();
  
  auto model = new QStringListModel(list, m_search_completer);
  m_search_completer->setModel(model);
}

void MainWindow::onInvoiceSelected(int row) {
  auto item = m_invoices_table->item(row, 0);
  if (!item)
    return;

  int invoice_id = item->text().toInt();
  if (invoice_id == m_active_invoice_id)
    return;

  if (m_is_dirty) {
    auto res = QMessageBox::warning(
        this, "Unsaved Changes",
        "You have unsaved changes on the current work order. Do you want to "
        "save them before switching?",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (res == QMessageBox::Yes) {
      onSaveInvoiceChanges();
      if (m_is_dirty) { // Save failed/blocked
        m_invoices_table->blockSignals(true);
        for (int i = 0; i < m_invoices_table->rowCount(); ++i) {
          if (m_invoices_table->item(i, 0)->text().toInt() ==
              m_active_invoice_id) {
            m_invoices_table->selectRow(i);
            break;
          }
        }
        m_invoices_table->blockSignals(false);
        return;
      }
    } else if (res == QMessageBox::Cancel) {
      m_invoices_table->blockSignals(true);
      for (int i = 0; i < m_invoices_table->rowCount(); ++i) {
        if (m_invoices_table->item(i, 0)->text().toInt() ==
            m_active_invoice_id) {
          m_invoices_table->selectRow(i);
          break;
        }
      }
      m_invoices_table->blockSignals(false);
      return;
    }
  }

  loadInvoiceDetails(invoice_id);
}

void MainWindow::loadInvoiceDetails(int invoice_id) {
  m_active_invoice_id = invoice_id;

  m_nav_invoice_spin->blockSignals(true);
  m_nav_invoice_spin->setValue(invoice_id);
  m_nav_invoice_spin->blockSignals(false);

  Invoice inv;
  if (m_db->getInvoice(invoice_id, inv)) {
    m_supplies_removed = inv.supplies_removed;
    m_ticket_title_label->setText(
        QString("Ticket #%1 (%2 %3)")
            .arg(inv.id)
            .arg(inv.vehicle.year)
            .arg(QString::fromStdString(inv.vehicle.model)));
    m_ticket_status_label->setText(
        QString("Status: %1").arg(QString::fromStdString(inv.status)));
    m_ticket_type_combo->setCurrentText(
        QString::fromStdString(inv.ticket_type));
    m_ticket_mileage_in_edit->setText(QString::number(inv.mileage_in));
    m_ticket_mileage_out_edit->setText(QString::number(inv.mileage_out));
    m_billed_by_edit->setText(QString::fromStdString(inv.writer));

    // Side-by-side fields
    m_t_cust_first_edit->setText(
        QString::fromStdString(inv.customer.first_name));
    m_t_cust_last_edit->setText(QString::fromStdString(inv.customer.last_name));
    m_t_cust_phone_edit->setText(
        QString::fromStdString(inv.customer.phone_number));
    m_t_veh_plate_edit->setText(
        QString::fromStdString(inv.vehicle.license_plate));
    m_t_veh_model_edit->setText(QString::fromStdString(inv.vehicle.model));
    m_t_veh_engine_edit->setText(
        QString::fromStdString(inv.vehicle.engine_specs));

    // Notes category fields
    m_notes_internal->setText(QString::fromStdString(inv.internal_notes));
    m_notes_customer->setText(QString::fromStdString(inv.customer_notes));
    m_notes_tech->setText(QString::fromStdString(inv.tech_notes));
    m_notes_vehicle->setText(QString::fromStdString(inv.vehicle_notes));
    m_notes_auth->setText(QString::fromStdString(inv.auth_notes));

    // Load vehicle service history
    m_history_list->setRowCount(0);
    auto history = m_db->getVehicleServiceHistory(inv.vehicle_id);
    for (const auto &h_inv : history) {
      int row = m_history_list->rowCount();
      m_history_list->insertRow(row);

      m_history_list->setItem(
          row, 0,
          new QTableWidgetItem(QString::fromStdString(h_inv.date_created)));
      m_history_list->setItem(
          row, 1,
          new QTableWidgetItem(QString::fromStdString(h_inv.ticket_type)));

      double parts_tot = 0;
      double labor_tot = 0;
      for (const auto &item : h_inv.items) {
        if (QString::fromStdString(item.item_type).trimmed().toLower() ==
            "part") {
          parts_tot += item.quantity * (item.unit_price / 100.0);
        } else {
          labor_tot += item.quantity * (item.unit_price / 100.0);
        }
      }
      double supplies = 0.0;
      if (labor_tot > 0) {
        supplies = labor_tot * 0.05;
        if (supplies < 2.0)
          supplies = 2.0;
        if (supplies > 25.0)
          supplies = 25.0;
      }
      double tax = parts_tot * m_sales_tax_rate;
      double grand = parts_tot + labor_tot + supplies + tax;

      m_history_list->setItem(row, 2,
                               new QTableWidgetItem(QString("$%1").arg(
                                   QString::number(grand, 'f', 2))));
    }

    // Pop items
    m_items_table->blockSignals(true);
    m_items_table->setRowCount(0);
    for (const auto &line : inv.items) {
      int row = m_items_table->rowCount();
      m_items_table->insertRow(row);

      // Type Combo widget
      auto type_combo = new QComboBox(m_items_table);
      type_combo->addItems({"Part", "Labor", "Fee", "Sublet", "Discount"});
      type_combo->setCurrentText(QString::fromStdString(line.item_type));
      m_items_table->setCellWidget(row, 0, type_combo);
      connect(type_combo, &QComboBox::currentTextChanged, this, &MainWindow::recalculateTicketTotals);

      m_items_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(line.part_number)));
      m_items_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(line.description)));
      m_items_table->setItem(row, 3, new QTableWidgetItem(QString::number(line.quantity)));

      double d_price = line.unit_price / 100.0;
      m_items_table->setItem(row, 4, new QTableWidgetItem(QString::number(d_price, 'f', 2)));

      double d_total = (line.quantity * line.unit_price) / 100.0;
      m_items_table->setItem(row, 5, new QTableWidgetItem(QString::number(d_total, 'f', 2)));

      // Tech Combo widget
      auto tech_combo = new QComboBox(m_items_table);
      tech_combo->addItems({"Office", "Bob (Tech)", "Jane (Tech)", "Al (Tech)"});
      tech_combo->setCurrentText(QString::fromStdString(line.tech_assigned));
      m_items_table->setCellWidget(row, 6, tech_combo);

      // Tax Checkable
      auto tax_item = new QTableWidgetItem();
      tax_item->setCheckState(line.taxable ? Qt::Checked : Qt::Unchecked);
      m_items_table->setItem(row, 7, tax_item);

      m_items_table->setItem(row, 8, new QTableWidgetItem(QString::fromStdString(line.line_notes)));
    }

    // Pad to at least 10 empty rows
    int min_rows = 10;
    while (m_items_table->rowCount() < min_rows) {
      int row = m_items_table->rowCount();
      m_items_table->insertRow(row);
      
      auto type_combo = new QComboBox(m_items_table);
      type_combo->addItems({"Part", "Labor", "Fee", "Sublet", "Discount"});
      type_combo->setCurrentText("Part");
      m_items_table->setCellWidget(row, 0, type_combo);
      connect(type_combo, &QComboBox::currentTextChanged, this, &MainWindow::recalculateTicketTotals);

      m_items_table->setItem(row, 1, new QTableWidgetItem(""));
      m_items_table->setItem(row, 2, new QTableWidgetItem(""));
      m_items_table->setItem(row, 3, new QTableWidgetItem(""));
      m_items_table->setItem(row, 4, new QTableWidgetItem(""));
      m_items_table->setItem(row, 5, new QTableWidgetItem(""));

      auto tech_combo = new QComboBox(m_items_table);
      tech_combo->addItems({"Office", "Bob (Tech)", "Jane (Tech)", "Al (Tech)"});
      tech_combo->setCurrentText("Office");
      m_items_table->setCellWidget(row, 6, tech_combo);

      auto tax_item = new QTableWidgetItem();
      tax_item->setCheckState(Qt::Checked);
      m_items_table->setItem(row, 7, tax_item);

      m_items_table->setItem(row, 8, new QTableWidgetItem(""));
    }
    m_items_table->blockSignals(false);

    // Signature Preview
    if (!inv.signature_data.empty()) {
        QByteArray ba = QByteArray::fromBase64(inv.signature_data.c_str());
        QImage img;
        img.loadFromData(ba, "PNG");
        if (!img.isNull()) {
            m_sig_preview_box->setPixmap(QPixmap::fromImage(img).scaled(m_sig_preview_box->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_sig_status_lbl->setText("Authorization: Signed");
        } else {
            m_sig_preview_box->setPixmap(QPixmap());
            m_sig_preview_box->setText("No signature capture found.");
            m_sig_status_lbl->setText("Authorization: Unsigned");
        }
    } else {
        m_sig_preview_box->setPixmap(QPixmap());
        m_sig_preview_box->setText("No signature capture found.");
        m_sig_status_lbl->setText("Authorization: Unsigned");
    }

    // Attachments Table
    m_attachments_table->setRowCount(0);
    auto attachs = m_db->getAttachments(invoice_id);
    for (const auto& a : attachs) {
        int r = m_attachments_table->rowCount();
        m_attachments_table->insertRow(r);
        m_attachments_table->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(a.file_name)));
        m_attachments_table->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(a.upload_time)));
        m_attachments_table->setItem(r, 2, new QTableWidgetItem(a.is_internal ? "Internal" : "Public"));
    }

    // Update Status pipeline UI selections
    updateStatusPipelineUI();

    recalculateTicketTotals();

    // Lock financial fields when the invoice is posted to the ledger
    // (posted_tx_id != 0). Notes fields are NOT locked here — they remain
    // editable even after finalization, since adding a note or correcting
    // contact info doesn't affect the posted totals. A voided invoice has
    // posted_tx_id == 0 and is therefore unlocked for correction.
    bool is_posted = (inv.posted_tx_id != 0);
    m_ticket_type_combo->setEnabled(!is_posted);
    m_ticket_mileage_in_edit->setEnabled(!is_posted);
    m_ticket_mileage_out_edit->setEnabled(!is_posted);

    m_t_cust_first_edit->setEnabled(!is_posted);
    m_t_cust_last_edit->setEnabled(!is_posted);
    m_t_cust_phone_edit->setEnabled(!is_posted);
    m_t_veh_plate_edit->setEnabled(!is_posted);
    m_t_veh_model_edit->setEnabled(!is_posted);
    m_t_veh_engine_edit->setEnabled(!is_posted);

    m_items_table->setEnabled(!is_posted);
    m_add_part_btn->setEnabled(!is_posted);
    m_add_labor_btn->setEnabled(!is_posted);
    m_remove_item_btn->setEnabled(!is_posted);
    m_insert_job_kit_btn->setEnabled(!is_posted);
    m_insert_catalog_btn->setEnabled(!is_posted);
    m_is_dirty = false;
  }
}

void MainWindow::onAddPartItem() {
  m_items_table->blockSignals(true);
  int row = m_items_table->rowCount();
  m_items_table->insertRow(row);
  m_items_table->setItem(row, 0, new QTableWidgetItem("PART-SKU"));
  m_items_table->setItem(row, 1, new QTableWidgetItem("New Part Description"));
  m_items_table->setItem(row, 2, new QTableWidgetItem("1"));
  m_items_table->setItem(row, 3, new QTableWidgetItem("0.00"));
  m_items_table->setItem(row, 4, new QTableWidgetItem("0.00"));
  m_items_table->setItem(row, 5, new QTableWidgetItem("Part"));
  m_items_table->blockSignals(false);
  recalculateTicketTotals();
  markDirty();
}

void MainWindow::onAddLaborItem() {
  m_items_table->blockSignals(true);
  int row = m_items_table->rowCount();
  m_items_table->insertRow(row);
  m_items_table->setItem(row, 0, new QTableWidgetItem("LABOR-CODE"));
  m_items_table->setItem(row, 1,
                         new QTableWidgetItem("Labor Service Description"));
  m_items_table->setItem(row, 2, new QTableWidgetItem("1.0"));
  m_items_table->setItem(row, 3, new QTableWidgetItem("0.00"));
  m_items_table->setItem(row, 4, new QTableWidgetItem("0.00"));
  m_items_table->setItem(row, 5, new QTableWidgetItem("Bob (Tech)"));
  m_items_table->blockSignals(false);
  recalculateTicketTotals();
  markDirty();
}

void MainWindow::onRemoveSelectedItems() {
  auto ranges = m_items_table->selectedRanges();
  if (ranges.isEmpty())
    return;
  m_items_table->removeRow(ranges.first().topRow());
  recalculateTicketTotals();
  markDirty();
}

void MainWindow::onSaveInvoiceChanges() {
  if (m_active_invoice_id == -1)
    return;

  std::string first = m_t_cust_first_edit->text().trimmed().toStdString();
  std::string last = m_t_cust_last_edit->text().trimmed().toStdString();
  std::string plate = m_t_veh_plate_edit->text().trimmed().toStdString();
  if (first.empty() || last.empty() || plate.empty()) {
    QMessageBox::critical(
        this, "Save Blocked",
        "Customer First/Last Name and Vehicle License Plate cannot be empty. "
        "Please enter or select a customer and vehicle to save.");
    return;
  }

  // Force commit of any active table cell editor
  m_items_table->setCurrentCell(-1, -1);

  // Read header details
  std::string type = m_ticket_type_combo->currentText().toStdString();
  int mileage_in = m_ticket_mileage_in_edit->text().toInt();
  int mileage_out = m_ticket_mileage_out_edit->text().toInt();

  // Update Customer & Vehicle details
  Invoice inv;
  bool cust_ok = true;
  bool veh_ok = true;
  if (m_db->getInvoice(m_active_invoice_id, inv)) {
    Customer c = inv.customer;
    c.first_name = m_t_cust_first_edit->text().trimmed().toStdString();
    c.last_name = m_t_cust_last_edit->text().trimmed().toStdString();
    c.phone_number = m_t_cust_phone_edit->text().trimmed().toStdString();
    cust_ok = m_db->updateCustomer(c);

    Vehicle v = inv.vehicle;
    v.license_plate = m_t_veh_plate_edit->text().trimmed().toStdString();
    v.model = m_t_veh_model_edit->text().trimmed().toStdString();
    v.engine_specs = m_t_veh_engine_edit->text().trimmed().toStdString();
    veh_ok = m_db->updateVehicle(v);
  }

  std::string writer = m_billed_by_edit->text().trimmed().toStdString();
  if (writer.empty()) writer = "Office";

  // Check current status or preserve active one
  std::string current_status = inv.status;
  if (current_status.empty()) current_status = "New";

  bool hdr_ok =
      m_db->updateInvoiceHeader(m_active_invoice_id, type, mileage_in,
                                mileage_out, current_status, m_supplies_removed, writer);

  // Save the separate notes categories
  std::string internal_notes = m_notes_internal->toPlainText().toStdString();
  std::string customer_notes = m_notes_customer->toPlainText().toStdString();
  std::string tech_notes = m_notes_tech->toPlainText().toStdString();
  std::string vehicle_notes = m_notes_vehicle->toPlainText().toStdString();
  std::string auth_notes = m_notes_auth->toPlainText().toStdString();
  m_db->updateInvoiceNotes(m_active_invoice_id, internal_notes, customer_notes, tech_notes, vehicle_notes, auth_notes);

  // Read lines details from 9-column grid
  std::vector<InvoiceItem> items;
  for (int i = 0; i < m_items_table->rowCount(); ++i) {
    auto type_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(i, 0));
    std::string item_type = type_combo ? type_combo->currentText().toStdString() : "Part";

    auto num_item = m_items_table->item(i, 1);
    auto desc_item = m_items_table->item(i, 2);
    auto qty_item = m_items_table->item(i, 3);
    auto price_item = m_items_table->item(i, 4);

    auto tech_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(i, 6));
    std::string tech_assigned = tech_combo ? tech_combo->currentText().toStdString() : "Office";

    bool taxable = (m_items_table->item(i, 7) && m_items_table->item(i, 7)->checkState() == Qt::Checked);
    auto note_item = m_items_table->item(i, 8);

    std::string part_num = num_item ? num_item->text().trimmed().toStdString() : "";
    std::string desc = desc_item ? desc_item->text().trimmed().toStdString() : "";
    if (part_num.empty() && desc.empty())
      continue;

    InvoiceItem item;
    item.invoice_id = m_active_invoice_id;
    item.part_number = part_num;
    item.description = desc;
    item.quantity = qty_item ? qty_item->text().toDouble() : 1.0;

    double price = price_item ? price_item->text().toDouble() : 0.0;
    item.unit_price = static_cast<int64_t>(price * 100.0);
    item.specification = item_type; // align with type
    item.item_type = item_type;
    item.tech_assigned = tech_assigned;
    item.taxable = taxable;
    item.line_notes = note_item ? note_item->text().trimmed().toStdString() : "";
    item.status = "Approved";

    items.push_back(item);
  }

  bool items_ok = m_db->saveInvoiceItems(m_active_invoice_id, items);

  if (!cust_ok || !veh_ok || !hdr_ok || !items_ok) {
    QMessageBox::critical(
        this, "Save Failed",
        QString("Failed to save all invoice details to the database:\n"
                "- Customer status: %1\n"
                "- Vehicle status: %2\n"
                "- Header status: %3\n"
                "- Items status: %4")
            .arg(cust_ok ? "OK" : "Failed")
            .arg(veh_ok ? "OK" : "Failed")
            .arg(hdr_ok ? "OK" : "Failed")
            .arg(items_ok ? "OK" : "Failed"));
    return;
  }

  // Auto add new parts to catalog
  auto existing_inv = m_db->getInventory();
  std::set<std::string> existing_parts;
  for (const auto& inv_item : existing_inv) {
    existing_parts.insert(inv_item.part_number);
  }

  for (const auto& item : items) {
    if (item.item_type == "Part" && !item.part_number.empty()) {
      if (existing_parts.find(item.part_number) == existing_parts.end()) {
        auto ans = QMessageBox::question(this, "New Part Number Detected",
            QString("Part number '%1' (%2) is not in the inventory catalog.\n"
                    "Would you like to save it to the catalog for later reference?")
            .arg(QString::fromStdString(item.part_number))
            .arg(QString::fromStdString(item.description)),
            QMessageBox::Yes | QMessageBox::No);
        if (ans == QMessageBox::Yes) {
          InventoryItem newItem;
          newItem.part_number = item.part_number;
          newItem.description = item.description;
          newItem.retail_price = item.unit_price;
          newItem.wholesale_cost = static_cast<int64_t>(item.unit_price * 0.7);
          newItem.quantity_on_hand = 0.0;
          newItem.reorder_point = 0.0;
          m_db->addInventoryItem(newItem);
          existing_parts.insert(item.part_number);
        }
      }
    }
  }

  QMessageBox::information(
      this, "Saved", "Invoice details, notes, and line items saved successfully.");
  loadInvoiceDetails(m_active_invoice_id);
  refreshInvoicesList();
}

void MainWindow::clearActiveInvoiceView() {
  m_active_invoice_id = -1;
  m_ticket_title_label->setText("Select a work order from left pane.");
  m_ticket_status_label->setText("");
  m_t_cust_first_edit->clear();
  m_t_cust_last_edit->clear();
  m_t_cust_phone_edit->clear();
  m_t_veh_plate_edit->clear();
  m_t_veh_model_edit->clear();
  m_t_veh_engine_edit->clear();
  m_ticket_mileage_in_edit->clear();
  m_ticket_mileage_out_edit->clear();
  m_billed_by_edit->clear();
  
  m_notes_internal->clear();
  m_notes_customer->clear();
  m_notes_tech->clear();
  m_notes_vehicle->clear();
  m_notes_auth->clear();

  m_sig_preview_box->setPixmap(QPixmap());
  m_sig_preview_box->setText("No signature capture found.");
  m_sig_status_lbl->setText("Authorization: Unsigned");

  m_attachments_table->setRowCount(0);

  m_items_table->blockSignals(true);
  m_items_table->setRowCount(0);
  int min_rows = 10;
  while (m_items_table->rowCount() < min_rows) {
    int row = m_items_table->rowCount();
    m_items_table->insertRow(row);
    
    auto type_combo = new QComboBox(m_items_table);
    type_combo->addItems({"Part", "Labor", "Fee", "Sublet", "Discount"});
    type_combo->setCurrentText("Part");
    m_items_table->setCellWidget(row, 0, type_combo);
    connect(type_combo, &QComboBox::currentTextChanged, this, &MainWindow::recalculateTicketTotals);

    m_items_table->setItem(row, 1, new QTableWidgetItem(""));
    m_items_table->setItem(row, 2, new QTableWidgetItem(""));
    m_items_table->setItem(row, 3, new QTableWidgetItem(""));
    m_items_table->setItem(row, 4, new QTableWidgetItem(""));
    m_items_table->setItem(row, 5, new QTableWidgetItem(""));

    auto tech_combo = new QComboBox(m_items_table);
    tech_combo->addItems({"Office", "Bob (Tech)", "Jane (Tech)", "Al (Tech)"});
    tech_combo->setCurrentText("Office");
    m_items_table->setCellWidget(row, 6, tech_combo);

    auto tax_item = new QTableWidgetItem();
    tax_item->setCheckState(Qt::Checked);
    m_items_table->setItem(row, 7, tax_item);

    m_items_table->setItem(row, 8, new QTableWidgetItem(""));
  }
  m_items_table->blockSignals(false);
  
  m_history_list->setRowCount(0);
  
  updateStatusPipelineUI();
  recalculateTicketTotals();

  m_invoices_table->blockSignals(true);
  m_invoices_table->clearSelection();
  m_invoices_table->blockSignals(false);
}

void MainWindow::onAddNewVehicleToCustomer() {
  std::string first = m_t_cust_first_edit->text().trimmed().toStdString();
  std::string last = m_t_cust_last_edit->text().trimmed().toStdString();
  if (first.empty() || last.empty()) {
    QMessageBox::warning(this, "Customer Required",
                         "Please enter Customer First and Last name first to "
                         "attach the vehicle to.");
    return;
  }

  AddVehicleDialog dlg(m_db, this);
  if (dlg.exec() != QDialog::Accepted)
    return;

  // Get active customer ID or create a new one
  int customer_id = -1;
  if (m_active_invoice_id != -1) {
    Invoice inv;
    if (m_db->getInvoice(m_active_invoice_id, inv)) {
      customer_id = inv.customer_id;
    }
  }

  if (customer_id == -1) {
    // Find existing customer or create a new one
    Customer c;
    c.first_name = first;
    c.last_name = last;
    c.phone_number = m_t_cust_phone_edit->text().trimmed().toStdString();

    // Let's check if customer already exists in db by name/phone
    auto list = m_db->getAllInvoices();
    for (const auto &inv : list) {
      if (inv.customer.first_name == first && inv.customer.last_name == last) {
        customer_id = inv.customer_id;
        break;
      }
    }
    if (customer_id == -1) {
      customer_id = m_db->insertCustomer(c);
    }
  }

  if (customer_id == -1) {
    QMessageBox::critical(this, "Error",
                          "Failed to create/resolve customer in the database.");
    return;
  }

  // Insert vehicle
  Vehicle v;
  v.customer_id = customer_id;
  v.license_plate = dlg.licensePlate();
  v.year = dlg.year();
  v.model = dlg.make() + " " + dlg.model();
  v.engine_specs = dlg.engineSpecs();
  int vehicle_id = m_db->insertVehicle(v);

  if (vehicle_id == -1) {
    QMessageBox::critical(this, "Error",
                          "Failed to save vehicle details to database.");
    return;
  }

  // Attach to active invoice or create a new invoice
  if (m_active_invoice_id != -1) {
    m_db->updateInvoiceVehicle(m_active_invoice_id, vehicle_id);
    loadInvoiceDetails(m_active_invoice_id);
  } else {
    std::string date =
        QDateTime::currentDateTime().toString("yyyy-MM-dd").toStdString();
    int inv_id =
        m_db->createInvoice(customer_id, vehicle_id, "Estimate", 0, date);
    if (inv_id != -1) {
      refreshInvoicesList();
      loadInvoiceDetails(inv_id);
    }
  }

  QMessageBox::information(
      this, "Vehicle Attached",
      "New vehicle details added and attached successfully.");
}

void MainWindow::onPrintTraveler() {
  if (m_active_invoice_id == -1)
    return;

  Invoice inv;
  if (!m_db->getInvoice(m_active_invoice_id, inv))
    return;

  PrintTemplate tmpl = m_db->getActivePrintTemplate();
  if (tmpl.content_html.empty()) {
    QMessageBox::critical(this, "Print Error",
                          "No active print template configured. Go to Setup -> "
                          "Print Templates Settings first.");
    return;
  }

  QString default_filename = QString("Print_Ticket_%1.pdf").arg(inv.id);
  QString save_path = QFileDialog::getSaveFileName(
      this, "Save Print Output PDF", QDir::current().filePath(default_filename),
      "PDF Files (*.pdf)");

  if (!save_path.isEmpty()) {
    if (TravelerRenderer::printToPDF(inv, tmpl.content_html,
                                     save_path.toStdString(),
                                     m_sales_tax_rate,
                                     m_supplies_percent,
                                     m_supplies_cap_cents)) {
      QMessageBox::information(
          this, "Print Success",
          QString("Work Order printed successfully using template '%1' to:\n%2")
              .arg(QString::fromStdString(tmpl.name))
              .arg(save_path));
    } else {
      QMessageBox::critical(this, "Print Error",
                            "Failed to render PDF document.");
    }
  }
}

void MainWindow::onFinalizeInvoice() {
  if (m_active_invoice_id == -1)
    return;

  // Idempotency pre-check (audit C6). If already posted, refuse and tell the
  // user to void first if a correction is needed.
  {
    Invoice inv;
    if (m_db->getInvoice(m_active_invoice_id, inv) && inv.posted_tx_id != 0) {
      QMessageBox::information(this, "Already Posted",
          QString("Invoice #%1 is already posted to the ledger (transaction #%2).\n\n"
                  "To make changes, void the invoice first — that will post a "
                  "reversing entry and unlock it for correction.")
              .arg(inv.id).arg(inv.posted_tx_id));
      return;
    }
  }

  // Warn about finalizing
  auto res = QMessageBox::question(
      this, "Confirm Finalize",
      "Are you sure you want to finalize this work order? This will "
      "permanently close the ticket and write transactional splits to the "
      "accounting ledger.\n\nFinancial fields will be locked. Corrections "
      "after this point require voiding the invoice (which posts a reversing "
      "entry).",
      QMessageBox::Yes | QMessageBox::No);
  if (res != QMessageBox::Yes)
    return;

  // Prompt for parts cost (to account for Parts COGS / Inventory Reduction)
  bool ok = false;
  double cogs_val =
      QInputDialog::getDouble(this, "Wholesale Parts Cost",
                              "Enter wholesale parts purchase cost (COGS) to "
                              "reduce Parts Inventory Asset:",
                              0.00, 0.00, 100000.00, 2, &ok);
  if (!ok)
    return;

  int64_t cogs_cents = static_cast<int64_t>(cogs_val * 100.0);

  // Save active state before final step
  onSaveInvoiceChanges();

  // Convert the configured tax rate (stored as a double like 0.0825) to basis
  // points for the integer-math finalize path. Round to nearest.
  int tax_rate_bps = static_cast<int>(m_sales_tax_rate * 10000.0 + 0.5);
  if (m_db->finalizeInvoice(m_active_invoice_id, cogs_cents, tax_rate_bps,
                            m_supplies_removed)) {
    // Auto-deduct inventory counts. Classification now uses item_type (audit
    // H3 unification), matching finalizeInvoice's classification.
    Invoice final_inv;
    if (m_db->getInvoice(m_active_invoice_id, final_inv)) {
      auto inventory = m_db->getInventory();
      for (const auto &line_item : final_inv.items) {
        if (QString::fromStdString(line_item.item_type)
                .trimmed()
                .toLower() == "part") {
          for (auto &inv_item : inventory) {
            if (inv_item.part_number == line_item.part_number) {
              inv_item.quantity_on_hand -= line_item.quantity;
              m_db->updateInventoryItem(inv_item);
              break;
            }
          }
        }
      }
      refreshInventoryData();
    }
    QMessageBox::information(this, "Ticket Closed",
                             "Work order has been finalized and accounting "
                             "ledger records posted successfully.");
    loadInvoiceDetails(m_active_invoice_id);
    refreshInvoicesList();
    refreshAccountingData();
  } else {
    QMessageBox::critical(
        this, "Error",
        "Failed to finalize work order. Please check system logs.");
  }
}

void MainWindow::refreshAccountingData() {
  // Refresh Accounts chart.
  //
  // Storage convention: debits are positive, credits are negative (matches
  // GnuCash's internal model). That means Liability/Income/Equity accounts,
  // which normally carry a credit balance, display as negative numbers under
  // a raw SUM(amount). For human readability we flip the sign on those account
  // types at display time only — the underlying splits are unchanged.
  auto accounts = m_db->getAccounts();
  m_accounts_table->setRowCount(0);
  for (const auto &a : accounts) {
    int row = m_accounts_table->rowCount();
    m_accounts_table->insertRow(row);

    auto name_item = new QTableWidgetItem(QString::fromStdString(a.name));
    name_item->setData(Qt::UserRole, a.id);
    m_accounts_table->setItem(row, 0, name_item);
    m_accounts_table->setItem(
        row, 1, new QTableWidgetItem(QString::fromStdString(a.type)));

    // Asset/Expense: debit-normal → display as-is.
    // Liability/Income/Equity: credit-normal → flip sign for display.
    int64_t display_balance = a.balance;
    if (a.type == "Liability" || a.type == "Income" || a.type == "Equity") {
      display_balance = -a.balance;
    }
    m_accounts_table->setItem(row, 2,
                              new QTableWidgetItem(formatCents(display_balance)));
  }

  // Refresh transactions
  auto txs = m_db->getTransactions();
  m_transactions_table->setRowCount(0);
  for (const auto &tx : txs) {
    for (size_t i = 0; i < tx.splits.size(); ++i) {
      int row = m_transactions_table->rowCount();
      m_transactions_table->insertRow(row);

      if (i == 0) {
        m_transactions_table->setItem(
            row, 0, new QTableWidgetItem(QString::fromStdString(tx.date)));
        m_transactions_table->setItem(
            row, 1,
            new QTableWidgetItem(QString::fromStdString(tx.description)));
      } else {
        m_transactions_table->setItem(row, 0, new QTableWidgetItem(""));
        m_transactions_table->setItem(row, 1, new QTableWidgetItem(""));
      }

      const auto &split = tx.splits[i];
      m_transactions_table->setItem(
          row, 2,
          new QTableWidgetItem(QString::fromStdString(split.account_name)));
      m_transactions_table->setItem(
          row, 3, new QTableWidgetItem(formatCents(split.amount)));
    }
  }
}

void MainWindow::onExportLedgerToCSV() {
  QString filename = QFileDialog::getSaveFileName(
      this, "Export General Ledger to CSV", QDir::current().filePath("general_ledger.csv"),
      "CSV Files (*.csv)");
  if (filename.isEmpty())
    return;

  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::critical(this, "Export Error", "Could not open file for writing.");
    return;
  }

  QTextStream out(&file);
  out << "Transaction ID,Date,Description,Account,Amount\n";

  auto txs = m_db->getTransactions();
  for (const auto &tx : txs) {
    for (const auto &split : tx.splits) {
      double dollar_amt = split.amount / 100.0;
      QString desc = QString::fromStdString(tx.description).replace("\"", "\"\"");
      QString acct = QString::fromStdString(split.account_name).replace("\"", "\"\"");
      
      out << QString("%1,%2,\"%3\",\"%4\",%5\n")
                 .arg(tx.id)
                 .arg(QString::fromStdString(tx.date))
                 .arg(desc)
                 .arg(acct)
                 .arg(QString::number(dollar_amt, 'f', 2));
    }
  }

  file.close();
  QMessageBox::information(this, "Export Complete", "General Ledger successfully exported to CSV.");
}

QString MainWindow::formatCents(int64_t cents) {
  double value = cents / 100.0;
  return QString("$%1").arg(QString::number(value, 'f', 2));
}

void MainWindow::recalculateTicketTotals() {
  double parts_total = 0.0;
  double labor_total = 0.0;
  double fees_total = 0.0;
  double discount_total = 0.0;
  double taxable_parts = 0.0;

  m_items_table->blockSignals(true);
  for (int i = 0; i < m_items_table->rowCount(); ++i) {
    auto type_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(i, 0));
    QString type = type_combo ? type_combo->currentText() : "Part";

    auto qty_item = m_items_table->item(i, 3);
    auto price_item = m_items_table->item(i, 4);
    if (!qty_item || !price_item)
      continue;

    double qty = qty_item->text().toDouble();
    double price = price_item->text().toDouble();
    double line_total = qty * price;

    // Write row total
    m_items_table->setItem(
        i, 5, new QTableWidgetItem(QString::number(line_total, 'f', 2)));

    bool taxable = (m_items_table->item(i, 7) && m_items_table->item(i, 7)->checkState() == Qt::Checked);

    if (type == "Part") {
      parts_total += line_total;
      if (taxable) {
        taxable_parts += line_total;
      }
    } else if (type == "Labor") {
      labor_total += line_total;
    } else if (type == "Fee" || type == "Sublet") {
      fees_total += line_total;
    } else if (type == "Discount") {
      discount_total += line_total;
      // Show discount total as negative in grid
      m_items_table->setItem(
          i, 5, new QTableWidgetItem(QString::number(-line_total, 'f', 2)));
    }
  }
  m_items_table->blockSignals(false);

  // Supplies: configured percent of labor, capped
  double supplies = 0.0;
  if (labor_total > 0 && !m_supplies_removed) {
    supplies = labor_total * m_supplies_percent;
    double cap = m_supplies_cap_cents / 100.0;
    if (supplies > cap)
      supplies = cap;
  }

  double tax = taxable_parts * m_sales_tax_rate; // sales tax on parts
  double grand_total = parts_total + labor_total + supplies + fees_total + tax - discount_total;
  if (grand_total < 0.0) grand_total = 0.0;

  // Retrieve prepayment from db
  double prepayment = 0.0;
  if (m_active_invoice_id != -1) {
    Invoice inv;
    if (m_db->getInvoice(m_active_invoice_id, inv)) {
      prepayment = inv.prepayment_cents / 100.0;
    }
  }
  double balance = grand_total - prepayment;
  if (balance < 0.0) balance = 0.0;

  m_summary_parts_lbl->setText(QString("$%1").arg(QString::number(parts_total, 'f', 2)));
  m_summary_labor_lbl->setText(QString("$%1").arg(QString::number(labor_total, 'f', 2)));
  m_summary_supplies_lbl->setText(QString("$%1").arg(QString::number(supplies, 'f', 2)));
  
  double subtotal = parts_total + labor_total + supplies + fees_total - discount_total;
  if (subtotal < 0.0) subtotal = 0.0;
  m_summary_subtotal_lbl->setText(QString("$%1").arg(QString::number(subtotal, 'f', 2)));
  
  m_summary_tax_title_lbl->setText(
      QString("Sales Tax (%1% Parts):").arg(QString::number(m_sales_tax_rate * 100.0, 'f', 1)));
  m_summary_tax_lbl->setText(QString("$%1").arg(QString::number(tax, 'f', 2)));
  m_summary_prepaid_lbl->setText(QString("$%1").arg(QString::number(prepayment, 'f', 2)));
  m_summary_total_lbl->setText(QString("$%1").arg(QString::number(grand_total, 'f', 2)));
  m_summary_balance_lbl->setText(QString("$%1").arg(QString::number(balance, 'f', 2)));
}

void MainWindow::onInsertJobKit() {
  auto kits = m_db->getJobKits();
  if (kits.empty()) {
    QMessageBox::warning(this, "Empty Kits",
                         "No job kits configured in the database.");
    return;
  }

  QStringList options;
  for (const auto &kit : kits) {
    options << QString::fromStdString(kit.name);
  }

  bool ok = false;
  QString selected = QInputDialog::getItem(
      this, "Select Job Kit", "Choose a predefined package to insert:", options,
      0, false, &ok);
  if (!ok)
    return;

  // Find details
  int kit_id = -1;
  for (const auto &kit : kits) {
    if (QString::fromStdString(kit.name) == selected) {
      kit_id = kit.id;
      break;
    }
  }

  JobKit details;
  if (m_db->getJobKitDetails(kit_id, details)) {
    m_items_table->blockSignals(true);
    for (const auto &line : details.items) {
      int target_row = -1;
      for (int r = 0; r < m_items_table->rowCount(); ++r) {
        auto p_num = m_items_table->item(r, 0);
        auto p_desc = m_items_table->item(r, 1);
        bool is_empty = (!p_num || p_num->text().trimmed().isEmpty()) &&
                        (!p_desc || p_desc->text().trimmed().isEmpty());
        if (is_empty) {
          target_row = r;
          break;
        }
      }

      if (target_row == -1) {
        target_row = m_items_table->rowCount();
        m_items_table->insertRow(target_row);
      }

      m_items_table->setItem(target_row, 0,
                             new QTableWidgetItem(line.item_type == "Part"
                                                      ? "KIT-PART"
                                                      : "KIT-LABOR"));
      m_items_table->setItem(
          target_row, 1,
          new QTableWidgetItem(QString::fromStdString(line.description)));
      m_items_table->setItem(
          target_row, 2, new QTableWidgetItem(QString::number(line.quantity)));

      double d_price = line.unit_price / 100.0;
      m_items_table->setItem(
          target_row, 3,
          new QTableWidgetItem(QString::number(d_price, 'f', 2)));
      m_items_table->setItem(target_row, 4, new QTableWidgetItem("0.00"));
      m_items_table->setItem(
          target_row, 5,
          new QTableWidgetItem(QString::fromStdString(line.item_type)));
    }
    m_items_table->blockSignals(false);
    recalculateTicketTotals();
    markDirty();
  }
}

void MainWindow::setupInventoryTab() {
  auto main_layout = new QHBoxLayout(m_inventory_tab);
  auto splitter = new QSplitter(Qt::Horizontal, m_inventory_tab);
  main_layout->addWidget(splitter);

  // Left column: Inventory list
  auto left_widget = new QWidget(this);
  auto left_layout = new QVBoxLayout(left_widget);
  left_layout->addWidget(new QLabel("Current Parts Inventory Stock:", this));

  m_inventory_table = new QTableWidget(this);
  m_inventory_table->setColumnCount(6);
  m_inventory_table->setHorizontalHeaderLabels(
      {"Part #", "Description", "Stock Qty", "Reorder Pt", "Cost", "Retail"});
  m_inventory_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  m_inventory_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_inventory_table->setSelectionMode(QAbstractItemView::SingleSelection);
  left_layout->addWidget(m_inventory_table);
  splitter->addWidget(left_widget);

  // Right column: Form
  auto right_widget = new QWidget(this);
  auto right_layout = new QVBoxLayout(right_widget);

  auto form_group = new QGroupBox("Add / Edit Stock Item", this);
  auto form_layout = new QFormLayout(form_group);

  m_part_num_edit = new QLineEdit(this);
  m_part_desc_edit = new QLineEdit(this);
  m_part_qty_edit = new QLineEdit(this);
  m_part_reorder_edit = new QLineEdit(this);
  m_part_cost_edit = new QLineEdit(this);
  m_part_retail_edit = new QLineEdit(this);

  form_layout->addRow("Part Number (SKU):", m_part_num_edit);
  form_layout->addRow("Description:", m_part_desc_edit);
  form_layout->addRow("Quantity On Hand:", m_part_qty_edit);
  form_layout->addRow("Reorder Threshold:", m_part_reorder_edit);
  form_layout->addRow("Wholesale Cost ($):", m_part_cost_edit);
  form_layout->addRow("Retail Price ($):", m_part_retail_edit);
  right_layout->addWidget(form_group);

  auto btn_layout = new QHBoxLayout();
  m_add_inventory_btn = new QPushButton("Add New Part", this);
  m_update_inventory_btn = new QPushButton("Save/Update Part", this);
  auto m_reorder_report_btn = new QPushButton("📦 Run Restock Report", this);
  m_reorder_report_btn->setStyleSheet(
      "background-color: #d32f2f; color: white; font-weight: bold; padding: "
      "6px;");

  btn_layout->addWidget(m_add_inventory_btn);
  btn_layout->addWidget(m_update_inventory_btn);
  btn_layout->addWidget(m_reorder_report_btn);
  right_layout->addLayout(btn_layout);
  splitter->addWidget(right_widget);

  // Connects
  connect(m_inventory_table, &QTableWidget::cellClicked, this,
          [this](int row, int) { onInventorySelected(row); });
  connect(m_add_inventory_btn, &QPushButton::clicked, this,
          &MainWindow::onAddInventory);
  connect(m_update_inventory_btn, &QPushButton::clicked, this, [this]() {
    // Simple update trigger based on current selected item
    auto ranges = m_inventory_table->selectedRanges();
    if (ranges.isEmpty())
      return;
    int row = ranges.first().topRow();
    int item_id = m_inventory_table->item(row, 0)->data(Qt::UserRole).toInt();

    InventoryItem item;
    item.id = item_id;
    item.part_number = m_part_num_edit->text().trimmed().toStdString();
    item.description = m_part_desc_edit->text().trimmed().toStdString();
    item.quantity_on_hand = m_part_qty_edit->text().toDouble();
    item.reorder_point = m_part_reorder_edit->text().toDouble();
    item.wholesale_cost =
        static_cast<int64_t>(m_part_cost_edit->text().toDouble() * 100);
    item.retail_price =
        static_cast<int64_t>(m_part_retail_edit->text().toDouble() * 100);

    if (m_db->updateInventoryItem(item)) {
      QMessageBox::information(this, "Success", "Stock part details updated.");
      refreshInventoryData();
    }
  });

  connect(m_reorder_report_btn, &QPushButton::clicked, this, [this]() {
    auto items = m_db->getInventory();
    QStringList low_stock;
    for (const auto &item : items) {
      if (item.quantity_on_hand <= item.reorder_point) {
        low_stock << QString("• %1 (%2) - Stock: %3, Reorder: %4")
                         .arg(QString::fromStdString(item.part_number))
                         .arg(QString::fromStdString(item.description))
                         .arg(item.quantity_on_hand)
                         .arg(item.reorder_point);
      }
    }
    if (low_stock.isEmpty()) {
      QMessageBox::information(
          this, "Restock Report",
          "All inventory items are fully stocked above reorder points!");
    } else {
      QMessageBox::warning(
          this, "Restock Needed",
          QString("The following parts are at or below reorder points:\n\n%1")
              .arg(low_stock.join("\n")));
    }
  });

  refreshInventoryData();
}

void MainWindow::refreshInventoryData() {
  auto items = m_db->getInventory();
  m_inventory_table->setRowCount(0);

  for (const auto &item : items) {
    int row = m_inventory_table->rowCount();
    m_inventory_table->insertRow(row);

    auto num_item =
        new QTableWidgetItem(QString::fromStdString(item.part_number));
    num_item->setData(Qt::UserRole, item.id);
    m_inventory_table->setItem(row, 0, num_item);
    m_inventory_table->setItem(
        row, 1, new QTableWidgetItem(QString::fromStdString(item.description)));
    m_inventory_table->setItem(
        row, 2, new QTableWidgetItem(QString::number(item.quantity_on_hand)));
    m_inventory_table->setItem(
        row, 3, new QTableWidgetItem(QString::number(item.reorder_point)));
    m_inventory_table->setItem(
        row, 4, new QTableWidgetItem(formatCents(item.wholesale_cost)));
    m_inventory_table->setItem(
        row, 5, new QTableWidgetItem(formatCents(item.retail_price)));

    // InvoMax visual cues: Highlight low-stock parts in red/orange alert
    if (item.quantity_on_hand <= item.reorder_point) {
      for (int col = 0; col < 6; ++col) {
        m_inventory_table->item(row, col)->setBackground(QColor(255, 235, 235));
        m_inventory_table->item(row, col)->setForeground(QColor(211, 47, 47));
      }
    }
  }
}

void MainWindow::onAddInventory() {
  InventoryItem item;
  item.part_number = m_part_num_edit->text().trimmed().toStdString();
  item.description = m_part_desc_edit->text().trimmed().toStdString();
  item.quantity_on_hand = m_part_qty_edit->text().toDouble();
  item.reorder_point = m_part_reorder_edit->text().toDouble();
  item.wholesale_cost =
      static_cast<int64_t>(m_part_cost_edit->text().toDouble() * 100);
  item.retail_price =
      static_cast<int64_t>(m_part_retail_edit->text().toDouble() * 100);

  if (item.part_number.empty() || item.description.empty())
    return;

  if (m_db->addInventoryItem(item)) {
    m_part_num_edit->clear();
    m_part_desc_edit->clear();
    m_part_qty_edit->clear();
    m_part_reorder_edit->clear();
    m_part_cost_edit->clear();
    m_part_retail_edit->clear();
    refreshInventoryData();
  }
}

void MainWindow::onInventorySelected(int row) {
  auto part_num = m_inventory_table->item(row, 0)->text();
  auto desc = m_inventory_table->item(row, 1)->text();
  auto qty = m_inventory_table->item(row, 2)->text();
  auto reorder = m_inventory_table->item(row, 3)->text();

  double cost = m_inventory_table->item(row, 4)->text().remove('$').toDouble();
  double retail =
      m_inventory_table->item(row, 5)->text().remove('$').toDouble();

  m_part_num_edit->setText(part_num);
  m_part_desc_edit->setText(desc);
  m_part_qty_edit->setText(qty);
  m_part_reorder_edit->setText(reorder);
  m_part_cost_edit->setText(QString::number(cost, 'f', 2));
  m_part_retail_edit->setText(QString::number(retail, 'f', 2));
}

void MainWindow::setupSchedulerTab() {
  auto main_layout = new QHBoxLayout(m_scheduler_tab);
  auto splitter = new QSplitter(Qt::Horizontal, m_scheduler_tab);
  main_layout->addWidget(splitter);

  // Left pane
  auto left_widget = new QWidget(this);
  auto left_layout = new QVBoxLayout(left_widget);
  left_layout->addWidget(new QLabel("Service Bay Schedule Slots:", this));

  m_scheduler_table = new QTableWidget(this);
  m_scheduler_table->setColumnCount(6);
  m_scheduler_table->setHorizontalHeaderLabels({"ID", "Bay Name", "Customer",
                                                "Vehicle Info",
                                                "Scheduled Time", "Notes"});
  m_scheduler_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  m_scheduler_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_scheduler_table->setSelectionMode(QAbstractItemView::SingleSelection);
  left_layout->addWidget(m_scheduler_table);
  splitter->addWidget(left_widget);

  // Right pane: booking
  auto right_widget = new QWidget(this);
  auto right_layout = new QVBoxLayout(right_widget);

  auto book_group = new QGroupBox("Book Bay Time-Slot", this);
  auto form_layout = new QFormLayout(book_group);

  m_schedule_bay_combo = new QComboBox(this);
  m_schedule_cust_edit = new QLineEdit(this);
  m_schedule_veh_edit = new QLineEdit(this);
  m_schedule_time_edit = new QLineEdit(this);
  m_schedule_time_edit->setPlaceholderText("YYYY-MM-DD HH:MM");
  m_schedule_notes_edit = new QLineEdit(this);

  form_layout->addRow("Select Service Bay:", m_schedule_bay_combo);
  form_layout->addRow("Customer Name:", m_schedule_cust_edit);
  form_layout->addRow("Vehicle specs:", m_schedule_veh_edit);
  form_layout->addRow("Scheduled Time:", m_schedule_time_edit);
  form_layout->addRow("Intake Notes:", m_schedule_notes_edit);
  right_layout->addWidget(book_group);

  auto btn_layout = new QHBoxLayout();
  m_add_schedule_btn = new QPushButton("Reserve Bay Slot", this);
  m_add_schedule_btn->setStyleSheet("background-color: #2e7d32; color: white; "
                                    "font-weight: bold; padding: 6px;");
  m_delete_schedule_btn = new QPushButton("Cancel Reservation", this);
  m_delete_schedule_btn->setStyleSheet("padding: 6px;");

  btn_layout->addWidget(m_delete_schedule_btn);
  btn_layout->addWidget(m_add_schedule_btn);
  right_layout->addLayout(btn_layout);
  splitter->addWidget(right_widget);

  // Connects
  connect(m_add_schedule_btn, &QPushButton::clicked, this,
          &MainWindow::onAddSchedule);
  connect(m_delete_schedule_btn, &QPushButton::clicked, this,
          &MainWindow::onDeleteSchedule);

  refreshSchedulerData();
}

void MainWindow::refreshSchedulerData() {
  // Populate Bays Combo
  m_schedule_bay_combo->clear();
  auto bays = m_db->getBays();
  for (const auto &b : bays) {
    m_schedule_bay_combo->addItem(QString::fromStdString(b.name), b.id);
  }

  // Populate Table
  auto schedules = m_db->getBaySchedules();
  m_scheduler_table->setRowCount(0);
  for (const auto &sched : schedules) {
    int row = m_scheduler_table->rowCount();
    m_scheduler_table->insertRow(row);

    m_scheduler_table->setItem(row, 0,
                               new QTableWidgetItem(QString::number(sched.id)));
    m_scheduler_table->setItem(
        row, 1, new QTableWidgetItem(QString::fromStdString(sched.bay_name)));
    m_scheduler_table->setItem(
        row, 2,
        new QTableWidgetItem(QString::fromStdString(sched.customer_name)));
    m_scheduler_table->setItem(
        row, 3,
        new QTableWidgetItem(QString::fromStdString(sched.vehicle_info)));
    m_scheduler_table->setItem(
        row, 4, new QTableWidgetItem(QString::fromStdString(sched.time_slot)));
    m_scheduler_table->setItem(
        row, 5, new QTableWidgetItem(QString::fromStdString(sched.notes)));
  }
}

void MainWindow::onAddSchedule() {
  int bay_id = m_schedule_bay_combo->currentData().toInt();
  QString customer = m_schedule_cust_edit->text().trimmed();
  QString vehicle = m_schedule_veh_edit->text().trimmed();
  QString time = m_schedule_time_edit->text().trimmed();
  QString notes = m_schedule_notes_edit->text().trimmed();

  if (customer.isEmpty() || vehicle.isEmpty() || time.isEmpty()) {
    QMessageBox::warning(this, "Validation Error",
                         "Customer, Vehicle, and Time Slot are required.");
    return;
  }

  BaySchedule sched;
  sched.bay_id = bay_id;
  sched.invoice_id = 0; // stand-alone appointment
  sched.customer_name = customer.toStdString();
  sched.vehicle_info = vehicle.toStdString();
  sched.time_slot = time.toStdString();
  sched.notes = notes.toStdString();

  if (m_db->addBaySchedule(sched)) {
    m_schedule_cust_edit->clear();
    m_schedule_veh_edit->clear();
    m_schedule_time_edit->clear();
    m_schedule_notes_edit->clear();
    refreshSchedulerData();
  }
}

void MainWindow::onDeleteSchedule() {
  auto ranges = m_scheduler_table->selectedRanges();
  if (ranges.isEmpty())
    return;

  int row = ranges.first().topRow();
  int sched_id = m_scheduler_table->item(row, 0)->text().toInt();

  if (m_db->deleteBaySchedule(sched_id)) {
    refreshSchedulerData();
  }
}

void MainWindow::setupMenuBar() {
  auto bar = menuBar();

  // 1. File
  auto m_file = bar->addMenu("&File");
  auto act_new = m_file->addAction("&New Work Order");
  auto act_save = m_file->addAction("&Save Active Ticket");
  act_save->setShortcut(QKeySequence("Ctrl+S"));
  auto act_backup = m_file->addAction("&Backup Database");
  m_file->addSeparator();
  auto act_exit = m_file->addAction("E&xit");

  // 2. Customer
  auto m_cust = bar->addMenu("&Customer");
  auto act_lookup_c = m_cust->addAction("&Find A Customer");
  auto act_new_c = m_cust->addAction("&Add New Customer");
  m_cust->addSeparator();
  auto act_service_history = m_cust->addAction("Vehicle &Service History");
  auto act_quotes_lookup = m_cust->addAction("&Quotes & Estimates Lookup");

  // 3. Suppliers
  auto m_supp = bar->addMenu("&Suppliers");
  auto act_manage_s = m_supp->addAction("&Manage Suppliers");

  // 4. Reports
  auto m_rep = bar->addMenu("&Reports");
  auto act_daily_sales = m_rep->addAction("Daily &Sales Report");
  auto act_accounting_ledger = m_rep->addAction("Double-Entry &Ledger View");
  auto act_tech_commissions =
      m_rep->addAction("Technician &Commissions Report");

  // 5. Inventory
  auto m_inv = bar->addMenu("&Inventory");
  auto act_manage_i = m_inv->addAction("Manage &Inventory");
  auto act_lookup_cat = m_inv->addAction("Catalog &Lookup");

  // 6. Setup
  auto m_setup = bar->addMenu("&Setup");
  auto act_shop_info = m_setup->addAction("&Shop Information");
  auto act_job_kits = m_setup->addAction("&Predefined Job Kits");
  auto act_tax_setup = m_setup->addAction("&Tax Settings");
  auto act_print_templates = m_setup->addAction("&Print Templates Settings");
  auto act_toggle_theme = m_setup->addAction("Toggle &Light/Dark Theme");

  // 7. Print
  auto m_print = bar->addMenu("&Print");
  auto act_print_traveler = m_print->addAction("Print &Traveler (PDF)");
  act_print_traveler->setShortcut(QKeySequence("Ctrl+P"));
  auto act_print_sticker = m_print->addAction("Print &Windshield Sticker");

  // 8. Service Reminders
  auto m_rem = bar->addMenu("Service &Reminders");
  auto act_reminders = m_rem->addAction("Send &Reminders");

  // 9. Help
  auto m_help = bar->addMenu("&Help");
  auto act_about = m_help->addAction("&About TuxRepair");

  // Connect them to their corresponding slot actions
  connect(act_new, &QAction::triggered, this, [this]() {
    m_tab_widget->setCurrentIndex(0);
    m_intake_lookup_edit->setFocus();
  });
  connect(act_save, &QAction::triggered, this,
          &MainWindow::onSaveInvoiceChanges);
  connect(act_backup, &QAction::triggered, this, [this]() {
    QDir().mkdir("backups");
    QString backup_path =
        QString("backups/tuxrepair_backup_%1.db")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    if (QFile::copy("tuxrepair.db", backup_path)) {
      QMessageBox::information(
          this, "Backup Success",
          QString("Database successfully backed up to:\n%1").arg(backup_path));
    } else {
      QMessageBox::warning(this, "Backup Failed",
                           "Could not copy active database file. Ensure the "
                           "database file exists and isn't locked.");
    }
  });
  connect(act_exit, &QAction::triggered, this, &QMainWindow::close);

  // Setup menu connections
  connect(act_tax_setup, &QAction::triggered, this, [this]() {
    bool ok = false;
    double new_rate = QInputDialog::getDouble(
        this, "Parts Sales Tax Settings",
        "Enter parts sales tax rate (%):", m_sales_tax_rate * 100.0, 0.0, 100.0,
        2, &ok);
    if (ok) {
      m_sales_tax_rate = new_rate / 100.0;
      m_db->setSetting("sales_tax_rate", std::to_string(m_sales_tax_rate));
      recalculateTicketTotals();
      QMessageBox::information(
          this, "Tax Config Updated",
          QString("Parts sales tax rate updated to %1%").arg(new_rate));
    }
  });
  connect(act_print_templates, &QAction::triggered, this, [this]() {
    TemplateEditorDialog dlg(m_db, this);
    dlg.exec();
  });
  connect(act_toggle_theme, &QAction::triggered, this, [this]() {
    m_dark_theme = !m_dark_theme;
    applyTheme();
  });

  // Shortcuts (Ctrl+F / F2 / Ctrl+N / F5 / F9 / F12)
  auto shortcut_focus_search = new QShortcut(QKeySequence("Ctrl+F"), this);
  connect(shortcut_focus_search, &QShortcut::activated, this, [this]() {
    m_tab_widget->setCurrentIndex(0);
    m_intake_lookup_edit->setFocus();
  });

  auto shortcut_f2 = new QShortcut(QKeySequence("F2"), this);
  connect(shortcut_f2, &QShortcut::activated, this, [this]() {
    m_tab_widget->setCurrentIndex(0);
    m_intake_lookup_edit->setFocus();
  });

  auto shortcut_ctrl_n = new QShortcut(QKeySequence("Ctrl+N"), this);
  connect(shortcut_ctrl_n, &QShortcut::activated, this, [this]() {
    clearActiveInvoiceView();
    m_t_cust_first_edit->setFocus();
  });

  auto shortcut_f5 = new QShortcut(QKeySequence("F5"), this);
  connect(shortcut_f5, &QShortcut::activated, this, [this]() {
    onPrintTraveler();
  });

  auto shortcut_f9 = new QShortcut(QKeySequence("F9"), this);
  connect(shortcut_f9, &QShortcut::activated, this, [this]() {
    onFinalizeInvoice();
  });

  auto shortcut_done = new QShortcut(QKeySequence("F12"), this);
  connect(shortcut_done, &QShortcut::activated, this, [this]() {
    if (m_tab_widget->currentIndex() == 0) {
      m_finalize_ticket_btn->showMenu();
    }
  });

  connect(act_lookup_c, &QAction::triggered, this, [this]() {
    CustomerLookupDialog dlg(m_db, this);
    if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
      int vehicle_id = dlg.selectedVehicle().id;
      int customer_id = dlg.selectedCustomer().id;
      auto invoices = m_db->getAllInvoices();
      int active_inv_id = -1;
      for (const auto &inv : invoices) {
        if (inv.vehicle_id == vehicle_id
            && inv.posted_tx_id == 0
            && inv.status != "Voided") {
          active_inv_id = inv.id;
          break;
        }
      }
      if (active_inv_id != -1) {
        loadInvoiceDetails(active_inv_id);
      } else {
        int inv_id = m_db->createInvoice(
            customer_id, vehicle_id, "Estimate", 0,
            QDateTime::currentDateTime().toString("yyyy-MM-dd").toStdString());
        if (inv_id != -1) {
          refreshInvoicesList();
          loadInvoiceDetails(inv_id);
        }
      }
    }
  });
  connect(act_new_c, &QAction::triggered, this, [this]() {
    clearActiveInvoiceView();
    m_t_cust_first_edit->setFocus();
  });

  connect(act_service_history, &QAction::triggered, this, [this]() {
    InvoiceLookupDialog dlg(false, m_db, this);
    if (dlg.exec() == QDialog::Accepted && dlg.selectedInvoiceId() != -1) {
      m_tab_widget->setCurrentIndex(0);
      loadInvoiceDetails(dlg.selectedInvoiceId());
    }
  });

  connect(act_quotes_lookup, &QAction::triggered, this, [this]() {
    InvoiceLookupDialog dlg(true, m_db, this);
    if (dlg.exec() == QDialog::Accepted && dlg.selectedInvoiceId() != -1) {
      m_tab_widget->setCurrentIndex(0);
      loadInvoiceDetails(dlg.selectedInvoiceId());
    }
  });

  connect(act_manage_s, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, "Suppliers Manager",
                             "Suppliers registry is fully managed via Accounts "
                             "Payable subledger in the Accounting Ledger tab.");
  });

  connect(act_daily_sales, &QAction::triggered, this,
          [this]() { m_tab_widget->setCurrentIndex(3); });
  connect(act_accounting_ledger, &QAction::triggered, this,
          [this]() { m_tab_widget->setCurrentIndex(3); });
  connect(act_tech_commissions, &QAction::triggered, this, [this]() {
    auto invoices = m_db->getAllInvoices();
    std::map<std::string, std::pair<double, double>> tech_stats;
    for (const auto &inv : invoices) {
      // Status string unified to 'Closed' (finalize writes 'Closed' now, not
      // 'Finalized'). posted_tx_id != 0 is the authoritative "is posted" check
      // but the UI status string is what the user sees in the list.
      if (inv.status != "Closed")
        continue;
      for (const auto &item : inv.items) {
        // Classification by item_type (audit H3). Only Labor lines contribute
        // to commissions; parts/fees/sublets/discounts are skipped.
        QString itype = QString::fromStdString(item.item_type).trimmed().toLower();
        if (itype != "labor")
          continue;
        QString tech = QString::fromStdString(item.tech_assigned).trimmed();
        if (tech.isEmpty())
          tech = "Unassigned";
        double hours = item.quantity;
        double rev = (item.quantity * item.unit_price) / 100.0;
        tech_stats[tech.toStdString()].first += hours;
        tech_stats[tech.toStdString()].second += rev;
      }
    }
    if (tech_stats.empty()) {
      QMessageBox::information(
          this, "Technician Commissions",
          "No technician labor data found on finalized invoices.");
      return;
    }
    QStringList report;
    report << "Technician Labor & Commission Summary:";
    report << "------------------------------------------";
    for (const auto &pair : tech_stats) {
      double hrs = pair.second.first;
      double rev = pair.second.second;
      double commission = rev * 0.40;
      report << QString("🔧 Tech: %1\n   • Hours Billed: %2 hrs\n   • Total "
                        "Labor Revenue: $%3\n   • Est. Commission (40%): $%4")
                    .arg(QString::fromStdString(pair.first))
                    .arg(QString::number(hrs, 'f', 1))
                    .arg(QString::number(rev, 'f', 2))
                    .arg(QString::number(commission, 'f', 2));
      report << "------------------------------------------";
    }
    QMessageBox::information(this, "Technician Commissions Report",
                             report.join("\n"));
  });

  connect(act_manage_i, &QAction::triggered, this,
          [this]() { m_tab_widget->setCurrentIndex(2); });
  connect(act_lookup_cat, &QAction::triggered, this, [this]() {
    CatalogLookupDialog dlg(m_db, this);
    dlg.exec();
  });

  connect(act_shop_info, &QAction::triggered, this, [this]() {
    QMessageBox::information(
        this, "Shop Information",
        "TuxRepair Open-Source Garage Management Suite\nSovereign Automotive "
        "Repair Shop Platform.");
  });
  connect(act_job_kits, &QAction::triggered, this,
          [this]() { onInsertJobKit(); });

  connect(act_print_traveler, &QAction::triggered, this,
          &MainWindow::onPrintTraveler);
  connect(act_print_sticker, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, "Windshield Stickers",
                             "Sticker template printed successfully on "
                             "connected thermal roll printer.");
  });

  connect(act_reminders, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, "Service Reminders",
                             "Scanned customer database. 0 service reminders "
                             "due for oil filter lube intervals today.");
  });

  connect(act_about, &QAction::triggered, this, [this]() {
    QMessageBox::about(this, "About TuxRepair",
                       "TuxRepair Suite v1.0.0\nOffline-first automotive "
                       "repair & accounting system.\nLicensed under AGPLv3.");
  });
}

void MainWindow::applyTheme() {
  if (m_dark_theme) {
    setStyleSheet(
        "QMainWindow { background-color: #1e1e1e; }"
        "QTabWidget::pane { border: 1px solid #333333; background-color: "
        "#2d2d2d; border-radius: 4px; }"
        "QTabBar::tab { background-color: #252526; border: 1px solid #333333; "
        "padding: 6px 12px; font-weight: bold; border-top-left-radius: 4px; "
        "border-top-right-radius: 4px; margin-right: 2px; color: #d4d4d4; }"
        "QTabBar::tab:selected { background-color: #2d2d2d; "
        "border-bottom-color: #2d2d2d; color: #569cd6; }"
        "QGroupBox { font-weight: bold; border: 1px solid #333333; "
        "border-radius: 6px; margin-top: 4px; padding-top: 8px; "
        "background-color: #252526; color: #d4d4d4; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 "
        "5px; color: #569cd6; }"
        "QTableWidget { gridline-color: #3f3f46; border: 1px solid #333333; "
        "background-color: #1e1e1e; color: #d4d4d4; "
        "selection-background-color: #264f78; selection-color: #ffffff; "
        "alternate-background-color: #252526; }"
        "QTableWidget::item { padding: 3px; }"
        "QHeaderView::section { background-color: #2d2d2d; padding: 4px; "
        "border: 1px solid #333333; font-weight: bold; color: #d4d4d4; }"
        "QLineEdit { border: 1px solid #3f3f46; border-radius: 4px; padding: "
        "3px; background-color: #1e1e1e; color: #d4d4d4; "
        "selection-background-color: #264f78; }"
        "QLineEdit:focus { border: 2px solid #569cd6; }"
        "QPushButton { border: 1px solid #3f3f46; border-radius: 4px; padding: "
        "4px 8px; background-color: #2d2d2d; color: #d4d4d4; min-height: 18px; "
        "font-weight: 500; }"
        "QPushButton:hover { background-color: #3e3e3f; border-color: #569cd6; "
        "}"
        "QPushButton:pressed { background-color: #1e1e1c; }"
        "QLabel { color: #d4d4d4; }");
  } else {
    setStyleSheet(
        "QMainWindow { background-color: #f5f6f8; }"
        "QTabWidget::pane { border: 1px solid #cfd8dc; background-color: "
        "white; border-radius: 4px; }"
        "QTabBar::tab { background-color: #eceff1; border: 1px solid #cfd8dc; "
        "padding: 6px 12px; font-weight: bold; border-top-left-radius: 4px; "
        "border-top-right-radius: 4px; margin-right: 2px; color: #37474f; }"
        "QTabBar::tab:selected { background-color: white; border-bottom-color: "
        "white; color: #1565c0; }"
        "QGroupBox { font-weight: bold; border: 1px solid #cfd8dc; "
        "border-radius: 6px; margin-top: 4px; padding-top: 8px; "
        "background-color: #fafafa; color: #37474f; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 "
        "5px; color: #1565c0; }"
        "QTableWidget { gridline-color: #e0e0e0; border: 1px solid #cfd8dc; "
        "background-color: white; color: black; selection-background-color: "
        "#bbdefb; selection-color: black; alternate-background-color: #f9f9f9; "
        "}"
        "QTableWidget::item { padding: 3px; }"
        "QHeaderView::section { background-color: #f0f4f8; padding: 4px; "
        "border: 1px solid #cfd8dc; font-weight: bold; color: #37474f; }"
        "QLineEdit { border: 1px solid #b0bec5; border-radius: 4px; padding: "
        "3px; background-color: white; color: black; "
        "selection-background-color: #b3e5fc; }"
        "QLineEdit:focus { border: 2px solid #1e88e5; }"
        "QPushButton { border: 1px solid #b0bec5; border-radius: 4px; padding: "
        "4px 8px; background-color: #ffffff; color: #37474f; min-height: 18px; "
        "font-weight: 500; }"
        "QPushButton:hover { background-color: #f5f5f5; border-color: #78909c; "
        "}"
        "QPushButton:pressed { background-color: #e0e0e0; }"
        "QLabel { color: black; }");
  }
}

void MainWindow::onItemsTableContextMenu(const QPoint &pos) {
  int row = m_items_table->rowAt(pos.y());
  if (row == -1)
    return;

  QMenu menu(this);
  auto act_part = menu.addAction("Convert to Part");
  auto act_labor = menu.addAction("Convert to Labor");
  menu.addSeparator();
  auto act_lookup = menu.addAction("Catalog Lookup...");
  auto act_clear = menu.addAction("Clear Row");
  menu.addSeparator();
  auto act_insert = menu.addAction("Insert Row Above");
  auto act_delete = menu.addAction("Delete Row");

  auto global_pos = m_items_table->mapToGlobal(pos);
  auto selected_act = menu.exec(global_pos);
  if (!selected_act)
    return;

  m_items_table->blockSignals(true);
  if (selected_act == act_part) {
    m_items_table->setItem(row, 0, new QTableWidgetItem("PART-SKU"));
    m_items_table->setItem(row, 1,
                           new QTableWidgetItem("New Part Description"));
    m_items_table->setItem(row, 2, new QTableWidgetItem("1"));
    m_items_table->setItem(row, 3, new QTableWidgetItem("0.00"));
    m_items_table->setItem(row, 4, new QTableWidgetItem("0.00"));
    m_items_table->setItem(row, 5, new QTableWidgetItem("Part"));
    markDirty();
    recalculateTicketTotals();
  } else if (selected_act == act_labor) {
    m_items_table->setItem(row, 0, new QTableWidgetItem("LABOR-CODE"));
    m_items_table->setItem(row, 1,
                           new QTableWidgetItem("Labor Service Description"));
    m_items_table->setItem(row, 2, new QTableWidgetItem("1.0"));
    m_items_table->setItem(row, 3, new QTableWidgetItem("0.00"));
    m_items_table->setItem(row, 4, new QTableWidgetItem("0.00"));
    m_items_table->setItem(row, 5, new QTableWidgetItem("Bob (Tech)"));
    markDirty();
    recalculateTicketTotals();
  } else if (selected_act == act_lookup) {
    CatalogLookupDialog dlg(m_db, this);
    if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
      m_items_table->setItem(row, 0, new QTableWidgetItem(dlg.selectedCode()));
      m_items_table->setItem(row, 1,
                             new QTableWidgetItem(dlg.selectedDescription()));
      m_items_table->setItem(row, 2, new QTableWidgetItem("1"));
      m_items_table->setItem(
          row, 3,
          new QTableWidgetItem(QString::number(dlg.selectedPrice(), 'f', 2)));
      m_items_table->setItem(row, 4, new QTableWidgetItem("0.00"));
      m_items_table->setItem(row, 5, new QTableWidgetItem(dlg.selectedType()));
      markDirty();
      recalculateTicketTotals();
    }
  } else if (selected_act == act_clear) {
    for (int col = 0; col < 6; ++col) {
      m_items_table->setItem(row, col, new QTableWidgetItem(""));
    }
    markDirty();
    recalculateTicketTotals();
  } else if (selected_act == act_insert) {
    m_items_table->insertRow(row);
    for (int col = 0; col < 6; ++col) {
      m_items_table->setItem(row, col, new QTableWidgetItem(""));
    }
    markDirty();
  } else if (selected_act == act_delete) {
    m_items_table->removeRow(row);
    markDirty();
    recalculateTicketTotals();
  }
  m_items_table->blockSignals(false);
}

void MainWindow::onStatusButtonClicked(int index) {
  if (m_active_invoice_id == -1)
    return;

  QStringList stages = {"New", "Intake", "Estimate", "Awaiting Approval", "Approved", 
                        "In Progress", "Waiting on Parts", "Ready", "Invoiced", "Closed"};
  if (index < 0 || index >= stages.size()) return;

  std::string new_status = stages[index].toStdString();

  // Only "Closed" posts to the ledger (runs onFinalizeInvoice). "Invoiced" is
  // a pure status flip — it no longer falsely claims to finalize.
  if (new_status == "Closed") {
      auto ans = QMessageBox::question(this, "Close & Finalize",
          QString("Transition this work order to 'Closed'?\n\n"
                  "This will post accounting splits to the ledger and lock "
                  "the financial fields. To change anything afterwards you "
                  "will need to void the invoice.")
              .arg(stages[index]), QMessageBox::Yes | QMessageBox::No);
      if (ans != QMessageBox::Yes) {
          updateStatusPipelineUI();
          return;
      }
      onFinalizeInvoice();
      return;
  }

  if (new_status == "Invoiced") {
      auto ans = QMessageBox::question(this, "Mark Invoiced",
          QString("Mark this work order as 'Invoiced'?\n\n"
                  "This is a status flag only — it does NOT post to the "
                  "ledger. Use 'Closed' when you want to finalize and post.")
              .arg(stages[index]), QMessageBox::Yes | QMessageBox::No);
      if (ans != QMessageBox::Yes) {
          updateStatusPipelineUI();
          return;
      }
  }

  // Update in DB
  Invoice inv;
  if (m_db->getInvoice(m_active_invoice_id, inv)) {
      m_db->updateInvoiceHeader(m_active_invoice_id, inv.ticket_type, inv.mileage_in, inv.mileage_out, new_status, inv.supplies_removed, inv.writer);
      m_db->addStatusHistoryEntry(m_active_invoice_id, new_status, inv.writer.empty() ? "Office" : inv.writer);
      loadInvoiceDetails(m_active_invoice_id);
  }
}

void MainWindow::updateStatusPipelineUI() {
  if (m_active_invoice_id == -1) {
      for (auto btn : m_status_buttons) {
          btn->setChecked(false);
          btn->setStyleSheet("padding: 6px; font-weight: bold; border: 1px solid #ccc; background-color: #f5f5f5; color: black;");
      }
      return;
  }

  Invoice inv;
  if (m_db->getInvoice(m_active_invoice_id, inv)) {
      const std::string status = inv.status;
      const bool is_voided = (status == "Voided");
      // A posted invoice has status='Closed' AND posted_tx_id != 0. We also
      // highlight 'Closed' for any posted invoice so the user sees at a glance
      // that the ledger was hit.
      const bool is_posted = (inv.posted_tx_id != 0);

      for (size_t i = 0; i < m_status_buttons.size(); ++i) {
          auto btn = m_status_buttons[i];
          const std::string label = btn->text().toStdString();
          const bool is_active_label = (label == status)
                                     || (is_posted && label == "Closed");

          if (is_voided) {
              // Dim all buttons for a voided invoice — it's out of the pipeline.
              btn->setChecked(false);
              btn->setStyleSheet("padding: 6px; font-weight: bold; "
                                 "border: 1px solid #999; background-color: #d0d0d0; "
                                 "color: #777;");
          } else if (is_active_label) {
              btn->setChecked(true);
              btn->setStyleSheet("padding: 6px; font-weight: bold; "
                                 "border: 1px solid #1976d2; background-color: #1976d2; "
                                 "color: white;");
          } else {
              btn->setChecked(false);
              btn->setStyleSheet("padding: 6px; font-weight: bold; "
                                 "border: 1px solid #ccc; background-color: #f5f5f5; "
                                 "color: black;");
          }
      }
  }
}

void MainWindow::onNewRO() {
  // Create a quick-customer & vehicle
  Customer c;
  c.first_name = "New";
  c.last_name = "WORK ORDER";
  int c_id = m_db->insertCustomer(c);
  if (c_id != -1) {
      Vehicle v;
      v.customer_id = c_id;
      v.license_plate = "TEMP-" + QString::number(QDateTime::currentMSecsSinceEpoch() % 10000).toStdString();
      v.model = "Pending Vehicle Intake";
      int v_id = m_db->insertVehicle(v);
      if (v_id != -1) {
          int inv_id = m_db->createInvoice(c_id, v_id, "Estimate", 0, QDateTime::currentDateTime().toString("yyyy-MM-dd").toStdString());
          if (inv_id != -1) {
              m_db->addStatusHistoryEntry(inv_id, "New", "Office");
              refreshInvoicesList();
              loadInvoiceDetails(inv_id);
              m_t_cust_first_edit->setFocus();
          }
      }
  }
}

void MainWindow::onSaveRO() {
  onSaveInvoiceChanges();
}

void MainWindow::onPrintRO() {
  if (m_active_invoice_id == -1) return;
  
  QStringList templates = {"Customer Estimate", "Technician Traveler", "Customer Invoice", "Internal RO view", "Payment Receipt"};
  bool ok = false;
  QString selected = QInputDialog::getItem(this, "Select Print Output View", "Output View:", templates, 0, false, &ok);
  if (ok && !selected.isEmpty()) {
      onPrintTraveler();
  }
}

void MainWindow::onSendEstimate() {
  if (m_active_invoice_id == -1) return;
  Invoice inv;
  if (m_db->getInvoice(m_active_invoice_id, inv)) {
      QMessageBox::information(this, "Estimate Sent", 
          QString("Simulating email/SMS dispatch of Estimate #%1 to %2 %3.")
          .arg(m_active_invoice_id)
          .arg(QString::fromStdString(inv.customer.first_name))
          .arg(QString::fromStdString(inv.customer.last_name)));
  }
}

void MainWindow::onApproveEstimate() {
  if (m_active_invoice_id == -1) return;

  SignaturePadDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
      std::string sig = dlg.signatureBase64().toStdString();
      m_db->updateInvoiceSignature(m_active_invoice_id, sig);
      
      bool ok = false;
      QString auth_name = QInputDialog::getText(this, "Authorization Name", "Authorized By (Customer Name):", QLineEdit::Normal, "", &ok);
      if (ok && !auth_name.isEmpty()) {
          m_notes_auth->setText("Approved by " + auth_name + " on " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
      }

      // Automatically move status to "Approved"
      Invoice inv;
      if (m_db->getInvoice(m_active_invoice_id, inv)) {
          m_db->updateInvoiceHeader(m_active_invoice_id, inv.ticket_type, inv.mileage_in, inv.mileage_out, "Approved", inv.supplies_removed, inv.writer);
          m_db->addStatusHistoryEntry(m_active_invoice_id, "Approved", auth_name.toStdString());
      }
      loadInvoiceDetails(m_active_invoice_id);
  }
}

void MainWindow::onConvertInvoice() {
  if (m_active_invoice_id == -1) return;
  m_ticket_type_combo->setCurrentText("Invoice");
  onSaveInvoiceChanges();
}

void MainWindow::onMarkReady() {
  if (m_active_invoice_id == -1) return;
  Invoice inv;
  if (m_db->getInvoice(m_active_invoice_id, inv)) {
      m_db->updateInvoiceHeader(m_active_invoice_id, inv.ticket_type, inv.mileage_in, inv.mileage_out, "Ready", inv.supplies_removed, inv.writer);
      m_db->addStatusHistoryEntry(m_active_invoice_id, "Ready", "Office");
      loadInvoiceDetails(m_active_invoice_id);
  }
}

void MainWindow::onCloseRO() {
  onFinalizeInvoice();
}

void MainWindow::onDuplicateRO() {
  if (m_active_invoice_id == -1) return;
  Invoice inv;
  if (m_db->getInvoice(m_active_invoice_id, inv)) {
      int inv_id = m_db->createInvoice(inv.customer_id, inv.vehicle_id, inv.ticket_type, inv.mileage_in, QDateTime::currentDateTime().toString("yyyy-MM-dd").toStdString());
      if (inv_id != -1) {
          m_db->saveInvoiceItems(inv_id, inv.items);
          m_db->addStatusHistoryEntry(inv_id, "New", "Office");
          refreshInvoicesList();
          loadInvoiceDetails(inv_id);
          QMessageBox::information(this, "RO Duplicated", QString("Work Order #%1 duplicated successfully as new Work Order #%2.").arg(inv.id).arg(inv_id));
      }
  }
}

void MainWindow::onVoidReopenRO() {
  if (m_active_invoice_id == -1) return;
  Invoice inv;
  if (!m_db->getInvoice(m_active_invoice_id, inv)) return;

  const bool is_posted   = (inv.posted_tx_id != 0);
  const bool is_voided   = (inv.status == "Voided");

  if (is_posted && !is_voided) {
    // Posted invoice → real void. Posts a reversing transaction to the ledger
    // and clears posted_tx_id so the invoice can be corrected and re-finalized.
    auto ans = QMessageBox::question(
        this, "Void Posted Invoice",
        QString("Void Invoice #%1?\n\n"
                "A reversing transaction will be posted to the accounting "
                "ledger. The original posting remains in the audit trail. "
                "The invoice will be unlocked for correction and "
                "re-finalization.")
            .arg(inv.id),
        QMessageBox::Yes | QMessageBox::No);
    if (ans != QMessageBox::Yes) return;
    if (m_db->voidInvoice(m_active_invoice_id)) {
      m_db->addStatusHistoryEntry(m_active_invoice_id, "Voided", "Office");
      QMessageBox::information(this, "Voided",
          QString("Invoice #%1 voided. A reversing entry was posted to the "
                  "ledger. The invoice is now editable; re-finalize when "
                  "corrected.").arg(inv.id));
    } else {
      QMessageBox::critical(this, "Error", "Failed to void invoice.");
    }
    loadInvoiceDetails(m_active_invoice_id);
    refreshInvoicesList();
    refreshAccountingData();
    return;
  }

  if (is_voided) {
    // Already voided → reopen as a draft. The void reversal stays in the
    // ledger as audit trail; we just flip status back so the user can edit
    // and re-finalize.
    auto ans = QMessageBox::question(
        this, "Reopen Invoice",
        QString("Reopen Invoice #%1 as 'New'?\n\n"
                "The previous void reversal remains in the ledger. You can "
                "edit the invoice and re-finalize it to post a new transaction.")
            .arg(inv.id),
        QMessageBox::Yes | QMessageBox::No);
    if (ans != QMessageBox::Yes) return;
    m_db->updateInvoiceHeader(m_active_invoice_id, inv.ticket_type,
                              inv.mileage_in, inv.mileage_out, "New",
                              inv.supplies_removed, inv.writer);
    m_db->addStatusHistoryEntry(m_active_invoice_id, "New", "Office");
    loadInvoiceDetails(m_active_invoice_id);
    refreshInvoicesList();
    return;
  }

  // Not posted, not voided → simple status toggle (legacy behavior). Lets the
  // user mark an unposted draft as "Voided" to take it out of the active list
  // without touching the ledger.
  std::string next_status = (inv.status == "Voided") ? "New" : "Voided";
  auto ans = QMessageBox::question(this, "Toggle Status",
      QString("Set Invoice #%1 status to '%2'? (Not posted — no ledger effect.)")
          .arg(inv.id).arg(QString::fromStdString(next_status)),
      QMessageBox::Yes | QMessageBox::No);
  if (ans == QMessageBox::Yes) {
    m_db->updateInvoiceHeader(m_active_invoice_id, inv.ticket_type,
                              inv.mileage_in, inv.mileage_out, next_status,
                              inv.supplies_removed, inv.writer);
    m_db->addStatusHistoryEntry(m_active_invoice_id, next_status, "Office");
    loadInvoiceDetails(m_active_invoice_id);
    refreshInvoicesList();
  }
}

void MainWindow::onRecordPayment() {
  if (m_active_invoice_id == -1) return;

  Invoice inv;
  if (!m_db->getInvoice(m_active_invoice_id, inv)) return;

  // Already voided → refuse to take money against it.
  if (inv.status == "Voided") {
    QMessageBox::warning(this, "Cannot Record Payment",
        "This invoice is voided. Reopen it before recording a payment.");
    return;
  }

  RecordPaymentDialog dlg(m_active_invoice_id, m_db, this);
  if (dlg.exec() != QDialog::Accepted) return;

  if (m_db->recordPayment(m_active_invoice_id, dlg.amountCents(),
                          dlg.method().toStdString(),
                          dlg.reference().toStdString())) {
    QString kind = (inv.posted_tx_id != 0)
        ? "Accounts Receivable payment"
        : "Customer Deposit (prepayment)";
    QMessageBox::information(this, "Payment Recorded",
        QString("Recorded %1 of $%2 as %3.")
            .arg(dlg.method())
            .arg(dlg.amountCents() / 100.0, 0, 'f', 2)
            .arg(kind));
    loadInvoiceDetails(m_active_invoice_id);  // refreshes prepayment/balance labels
    refreshAccountingData();                   // shows the new Cash/AR/Deposits movement
  } else {
    QMessageBox::critical(this, "Error", "Failed to record payment.");
  }
}

void MainWindow::onAddAttachment() {
  if (m_active_invoice_id == -1) return;
  QString path = QFileDialog::getOpenFileName(this, "Select File to Attach");
  if (!path.isEmpty()) {
      QFileInfo info(path);
      Attachment a;
      a.invoice_id = m_active_invoice_id;
      a.file_path = path.toStdString();
      a.file_name = info.fileName().toStdString();
      a.is_internal = true;
      m_db->addAttachment(a);
      loadInvoiceDetails(m_active_invoice_id);
  }
}

void MainWindow::onDeleteAttachment() {
  int r = m_attachments_table->currentRow();
  if (r == -1) return;
  auto attachs = m_db->getAttachments(m_active_invoice_id);
  if (r >= 0 && r < static_cast<int>(attachs.size())) {
      m_db->deleteAttachment(attachs[r].id);
      loadInvoiceDetails(m_active_invoice_id);
  }
}

void MainWindow::onIntakeSearch() {
  QString terms = m_intake_lookup_edit->text().trimmed();
  QString type = m_intake_lookup_type_combo->currentText();
  if (terms.isEmpty()) {
      refreshInvoicesList();
      return;
  }

  m_invoices_table->setRowCount(0);
  std::vector<Invoice> matches;

  if (type == "License Plate") {
      auto vehs = m_db->searchVehiclesByPlate(terms.toStdString());
      auto all_invs = m_db->getAllInvoices();
      for (const auto& v : vehs) {
          for (const auto& inv : all_invs) {
              if (inv.vehicle_id == v.id) {
                  matches.push_back(inv);
              }
          }
      }
  } else if (type == "VIN") {
      auto all_invs = m_db->getAllInvoices();
      for (const auto& inv : all_invs) {
          if (inv.vehicle.vin.find(terms.toStdString()) != std::string::npos) {
              matches.push_back(inv);
          }
      }
  } else if (type == "Phone") {
      auto custs = m_db->searchCustomers("phone_number", terms.toStdString());
      auto all_invs = m_db->getAllInvoices();
      for (const auto& c : custs) {
          for (const auto& inv : all_invs) {
              if (inv.customer_id == c.id) {
                  matches.push_back(inv);
              }
          }
      }
  } else if (type == "Email") {
      auto custs = m_db->searchCustomers("email", terms.toStdString());
      auto all_invs = m_db->getAllInvoices();
      for (const auto& c : custs) {
          for (const auto& inv : all_invs) {
              if (inv.customer_id == c.id) {
                  matches.push_back(inv);
              }
          }
      }
  } else {
      auto custs = m_db->searchCustomers("last_name", terms.toStdString());
      auto all_invs = m_db->getAllInvoices();
      for (const auto& c : custs) {
          for (const auto& inv : all_invs) {
              if (inv.customer_id == c.id) {
                  matches.push_back(inv);
              }
          }
      }
  }

  for (const auto& inv : matches) {
      int r = m_invoices_table->rowCount();
      m_invoices_table->insertRow(r);
      m_invoices_table->setItem(r, 0, new QTableWidgetItem(QString::number(inv.id)));
      m_invoices_table->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(inv.customer.first_name + " " + inv.customer.last_name)));
      m_invoices_table->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(inv.vehicle.license_plate)));
      m_invoices_table->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(inv.status)));
  }
}

void MainWindow::onCellChangedGrid(int row, int col) {
  if (col == 3 || col == 4) {
      // Re-trigger row totals calculation
      auto qty_item = m_items_table->item(row, 3);
      auto rate_item = m_items_table->item(row, 4);
      if (qty_item && rate_item) {
          double qty = qty_item->text().toDouble();
          double rate = rate_item->text().toDouble();
          m_items_table->setItem(row, 5, new QTableWidgetItem(QString::number(qty * rate, 'f', 2)));
      }
  }
  recalculateTicketTotals();
}

void MainWindow::onItemsTableCellClicked(int row, int col) {
  // Column 1 is SKU/Code in the new 9-column format
  if (col != 1)
    return;

  auto item = m_items_table->item(row, col);
  if (!item || item->text().trimmed().isEmpty()) {
    QMenu menu(this);
    auto act_part = menu.addAction("Initialize as Part");
    auto act_labor = menu.addAction("Initialize as Labor");
    auto act_lookup = menu.addAction("Select from Catalog...");

    QPoint global_pos = QCursor::pos();
    auto selected_act = menu.exec(global_pos);
    if (!selected_act)
      return;

    m_items_table->blockSignals(true);
    if (selected_act == act_part) {
      // Type Combo is at column 0
      auto type_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(row, 0));
      if (type_combo) type_combo->setCurrentText("Part");
      
      m_items_table->setItem(row, 1, new QTableWidgetItem("PART-SKU"));
      m_items_table->setItem(row, 2, new QTableWidgetItem("New Part Description"));
      m_items_table->setItem(row, 3, new QTableWidgetItem("1"));
      m_items_table->setItem(row, 4, new QTableWidgetItem("0.00"));
      m_items_table->setItem(row, 5, new QTableWidgetItem("0.00"));
      
      auto tech_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(row, 6));
      if (tech_combo) tech_combo->setCurrentText("Office");

      auto tax_item = new QTableWidgetItem();
      tax_item->setCheckState(Qt::Checked);
      m_items_table->setItem(row, 7, tax_item);

      m_items_table->setItem(row, 8, new QTableWidgetItem(""));

      markDirty();
      recalculateTicketTotals();
    } else if (selected_act == act_labor) {
      auto type_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(row, 0));
      if (type_combo) type_combo->setCurrentText("Labor");

      m_items_table->setItem(row, 1, new QTableWidgetItem("LABOR-CODE"));
      m_items_table->setItem(row, 2, new QTableWidgetItem("Labor Service Description"));
      m_items_table->setItem(row, 3, new QTableWidgetItem("1.0"));
      m_items_table->setItem(row, 4, new QTableWidgetItem("0.00"));
      m_items_table->setItem(row, 5, new QTableWidgetItem("0.00"));

      auto tech_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(row, 6));
      if (tech_combo) tech_combo->setCurrentText("Bob (Tech)");

      auto tax_item = new QTableWidgetItem();
      tax_item->setCheckState(Qt::Unchecked);
      m_items_table->setItem(row, 7, tax_item);

      m_items_table->setItem(row, 8, new QTableWidgetItem(""));

      markDirty();
      recalculateTicketTotals();
    } else if (selected_act == act_lookup) {
      CatalogLookupDialog dlg(m_db, this);
      if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
        auto type_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(row, 0));
        if (type_combo) type_combo->setCurrentText(dlg.selectedType());

        m_items_table->setItem(row, 1, new QTableWidgetItem(dlg.selectedCode()));
        m_items_table->setItem(row, 2, new QTableWidgetItem(dlg.selectedDescription()));
        m_items_table->setItem(row, 3, new QTableWidgetItem("1"));
        m_items_table->setItem(row, 4, new QTableWidgetItem(QString::number(dlg.selectedPrice(), 'f', 2)));
        m_items_table->setItem(row, 5, new QTableWidgetItem("0.00"));

        auto tech_combo = qobject_cast<QComboBox*>(m_items_table->cellWidget(row, 6));
        if (tech_combo) tech_combo->setCurrentText("Office");

        auto tax_item = new QTableWidgetItem();
        tax_item->setCheckState(Qt::Checked);
        m_items_table->setItem(row, 7, tax_item);

        m_items_table->setItem(row, 8, new QTableWidgetItem(""));

        markDirty();
        recalculateTicketTotals();
      }
    }
    m_items_table->blockSignals(false);
  }
}

} // namespace tuxrepair
