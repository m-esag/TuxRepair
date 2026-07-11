#ifndef TUXREPAIR_TEMPLATE_EDITOR_DIALOG_H
#define TUXREPAIR_TEMPLATE_EDITOR_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <memory>
#include "db_manager.h"
#include "models.h"

namespace tuxrepair {

class TemplateEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit TemplateEditorDialog(std::shared_ptr<DBManager> db, QWidget* parent = nullptr);

private slots:
    void onTemplateSelected(int index);
    void onCodeChanged();
    void onAddTemplate();
    void onDeleteTemplate();
    void onSetActiveTemplate();
    void onInsertPlaceholder(const QString& placeholder);

private:
    std::shared_ptr<DBManager> m_db;
    std::vector<PrintTemplate> m_templates;
    int m_current_index = -1;
    bool m_updating_code = false;

    QListWidget* m_list_widget;
    QTextEdit* m_code_edit;
    QTextBrowser* m_preview_browser;

    QPushButton* m_add_btn;
    QPushButton* m_delete_btn;
    QPushButton* m_set_active_btn;

    void refreshList();
    void loadCurrentTemplate();
    void updatePreview();
    Invoice getMockInvoice();
};

} // namespace tuxrepair

#endif // TUXREPAIR_TEMPLATE_EDITOR_DIALOG_H
