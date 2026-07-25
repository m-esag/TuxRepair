#ifndef TUXREPAIR_RECORD_PAYMENT_DIALOG_H
#define TUXREPAIR_RECORD_PAYMENT_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QDateEdit>
#include <memory>
#include "db_manager.h"

namespace tuxrepair {

// Dialog for recording a customer payment against an invoice.
//
// The caller invokes exec() and, on Accepted, reads amountCents()/method()/
// reference() and passes them to DBManager::recordPayment(). recordPayment
// decides based on the invoice's posted_tx_id whether to post as an A/R
// payment or a Customer Deposits prepayment — this dialog does not need to
// know which case it is.
class RecordPaymentDialog : public QDialog {
    Q_OBJECT
public:
    RecordPaymentDialog(int invoice_id, std::shared_ptr<DBManager> db,
                        QWidget* parent = nullptr);

    int64_t amountCents() const;
    QString method() const;
    QString reference() const;
    QDate   date() const;

private slots:
    void onSave();

private:
    std::shared_ptr<DBManager> m_db;
    int m_invoice_id;
    QDoubleSpinBox* m_amount_spin;
    QComboBox*      m_method_combo;
    QLineEdit*      m_reference_edit;
    QDateEdit*      m_date_edit;
};

} // namespace tuxrepair

#endif // TUXREPAIR_RECORD_PAYMENT_DIALOG_H
