#include "record_payment_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

namespace tuxrepair {

RecordPaymentDialog::RecordPaymentDialog(int invoice_id, std::shared_ptr<DBManager> db,
                                         QWidget* parent)
    : QDialog(parent), m_db(db), m_invoice_id(invoice_id) {

    setWindowTitle("Record Payment");
    resize(360, 220);

    auto layout = new QVBoxLayout(this);
    auto form = new QFormLayout();

    // Show which invoice this applies to and whether it's posted (so the user
    // understands whether this becomes an A/R payment or a Customer Deposit).
    {
        Invoice inv;
        bool posted = false;
        if (m_db && m_db->getInvoice(m_invoice_id, inv)) {
            posted = (inv.posted_tx_id != 0);
        }
        QString banner = QString("Invoice #%1 — %2")
            .arg(m_invoice_id)
            .arg(posted ? "Posted (will credit Accounts Receivable)"
                        : "Not posted (will record as Customer Deposit)");
        auto lbl = new QLabel(banner, this);
        lbl->setStyleSheet("font-weight: bold; color: #555; padding: 4px;");
        layout->addWidget(lbl);
    }

    m_amount_spin = new QDoubleSpinBox(this);
    m_amount_spin->setRange(0.01, 100000.00);
    m_amount_spin->setDecimals(2);
    m_amount_spin->setPrefix("$");
    m_amount_spin->setValue(0.00);
    m_amount_spin->setFocus();

    m_method_combo = new QComboBox(this);
    m_method_combo->addItems({"Cash", "Check", "Card", "Other"});

    m_reference_edit = new QLineEdit(this);
    m_reference_edit->setPlaceholderText("Optional — check #, transaction id, etc.");

    m_date_edit = new QDateEdit(QDate::currentDate(), this);
    m_date_edit->setCalendarPopup(true);
    m_date_edit->setDisplayFormat("yyyy-MM-dd");

    form->addRow("Amount:", m_amount_spin);
    form->addRow("Method:", m_method_combo);
    form->addRow("Reference:", m_reference_edit);
    form->addRow("Date:", m_date_edit);
    layout->addLayout(form);

    // Buttons
    auto btn_layout = new QHBoxLayout();
    auto save_btn = new QPushButton("Record Payment", this);
    save_btn->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 12px;");
    auto cancel_btn = new QPushButton("Cancel", this);
    cancel_btn->setStyleSheet("padding: 6px 12px;");
    btn_layout->addStretch();
    btn_layout->addWidget(save_btn);
    btn_layout->addWidget(cancel_btn);
    layout->addLayout(btn_layout);

    connect(save_btn, &QPushButton::clicked, this, &RecordPaymentDialog::onSave);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
}

void RecordPaymentDialog::onSave() {
    if (m_amount_spin->value() <= 0.0) {
        QMessageBox::warning(this, "Invalid Amount",
                             "Payment amount must be greater than zero.");
        return;
    }
    accept();
}

int64_t RecordPaymentDialog::amountCents() const {
    // Round to nearest cent — the spinbox already enforces 2 decimals but
    // doubles can still carry floating error, so add 0.5 before truncating.
    return static_cast<int64_t>(m_amount_spin->value() * 100.0 + 0.5);
}

QString RecordPaymentDialog::method() const {
    return m_method_combo->currentText();
}

QString RecordPaymentDialog::reference() const {
    return m_reference_edit->text().trimmed();
}

QDate RecordPaymentDialog::date() const {
    return m_date_edit->date();
}

} // namespace tuxrepair
