#ifndef TUXREPAIR_REGISTER_DIALOG_H
#define TUXREPAIR_REGISTER_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <memory>
#include "db_manager.h"

namespace tuxrepair {

class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    RegisterDialog(int account_id, const QString& account_name, std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

private:
    std::shared_ptr<DBManager> m_db;
    QTableWidget* m_register_table;
    
    void loadRegister(int account_id);
};

} // namespace tuxrepair

#endif // TUXREPAIR_REGISTER_DIALOG_H
