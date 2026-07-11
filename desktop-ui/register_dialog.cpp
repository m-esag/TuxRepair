#include "register_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QString>
#include <QTableWidgetItem>
#include <QBrush>
#include <QLabel>
#include <QPushButton>

namespace tuxrepair {

RegisterDialog::RegisterDialog(int account_id, const QString& account_name, std::shared_ptr<DBManager> db, QWidget* parent)
    : QDialog(parent), m_db(db) {
    setWindowTitle(QString("Account Register - %1").arg(account_name));
    resize(700, 480);

    setStyleSheet(
        "QDialog { background-color: #f5f6f8; }"
        "QTableWidget { gridline-color: #e0e0e0; border: 1px solid #cfd8dc; selection-background-color: #bbdefb; selection-color: black; alternate-background-color: #f9f9f9; }"
        "QHeaderView::section { background-color: #f0f4f8; padding: 6px; border: 1px solid #cfd8dc; font-weight: bold; color: #37474f; }"
        "QPushButton { border: 1px solid #b0bec5; border-radius: 4px; padding: 6px 12px; background-color: #ffffff; color: #37474f; min-height: 20px; font-weight: bold; }"
        "QPushButton:hover { background-color: #f5f5f5; border-color: #78909c; }"
    );

    auto layout = new QVBoxLayout(this);

    auto title_lbl = new QLabel(QString("Audit Log Ledger: %1").arg(account_name), this);
    title_lbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #1565c0;");
    layout->addWidget(title_lbl);

    m_register_table = new QTableWidget(this);
    m_register_table->setColumnCount(5);
    m_register_table->setHorizontalHeaderLabels({"Date", "Description", "Debit (+)", "Credit (-)", "Running Balance"});
    m_register_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_register_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_register_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_register_table);

    auto close_btn = new QPushButton("Close Register", this);
    close_btn->setStyleSheet("padding: 6px; font-weight: bold;");
    layout->addWidget(close_btn);

    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);

    loadRegister(account_id);
}

void RegisterDialog::loadRegister(int account_id) {
    auto rows = m_db->getAccountRegister(account_id);
    m_register_table->setRowCount(0);

    for (const auto& r : rows) {
        int row = m_register_table->rowCount();
        m_register_table->insertRow(row);

        m_register_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.date)));
        m_register_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(r.description)));

        // Debit / Credit formatting
        if (r.amount >= 0) {
            double val = r.amount / 100.0;
            m_register_table->setItem(row, 2, new QTableWidgetItem(QString("$%1").arg(QString::number(val, 'f', 2))));
            m_register_table->setItem(row, 3, new QTableWidgetItem(""));
        } else {
            double val = (-r.amount) / 100.0;
            m_register_table->setItem(row, 2, new QTableWidgetItem(""));
            m_register_table->setItem(row, 3, new QTableWidgetItem(QString("$%1").arg(QString::number(val, 'f', 2))));
        }

        // Running balance formatting
        double bal = r.balance / 100.0;
        auto bal_item = new QTableWidgetItem(QString("$%1").arg(QString::number(bal, 'f', 2)));
        if (bal < 0) {
            bal_item->setForeground(QBrush(Qt::red));
        }
        m_register_table->setItem(row, 4, bal_item);
    }
}

} // namespace tuxrepair
