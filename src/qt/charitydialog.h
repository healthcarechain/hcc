#ifndef CHARITYDIALOG_H
#define CHARITYDIALOG_H

#include <QDialog>

namespace Ui {
class StakeForCharityDialog;
}
class WalletModel;
class QLineEdit;
class StakeForCharityDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StakeForCharityDialog(QWidget *parent = 0);
    ~StakeForCharityDialog();

    void setModel(WalletModel *model);
	void setAddress(const QString &address);
	void setAddress(const QString &address, QLineEdit *addrEdit);
private slots:
	void on_viewButton_clicked();
    void on_addButton_clicked();
    void on_deleteButton_clicked();
	void on_activateButton_clicked();
	void on_disableButton_clicked();
	void on_addressBookButton_clicked();

	
private:
    Ui::StakeForCharityDialog *ui;
	WalletModel *model;
};

#endif // CHARITYDIALOG_H