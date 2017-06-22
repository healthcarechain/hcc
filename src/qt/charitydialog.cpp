#include "charitydialog.h"
#include "ui_charitydialog.h"

#include "walletmodel.h"
#include "base58.h"
#include "addressbookpage.h"
#include "init.h"

#include <QMessageBox>
#include <QLineEdit>

#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

StakeForCharityDialog::StakeForCharityDialog(QWidget *parent) :
    QDialog(parent, (Qt::WindowMinMaxButtonsHint|Qt::WindowCloseButtonHint)),
    ui(new Ui::StakeForCharityDialog),
	 model(0)
{
    ui->setupUi(this);
	 connect(ui->closeButton, SIGNAL(pressed()), this, SLOT(close()));
}

StakeForCharityDialog::~StakeForCharityDialog()
{
    delete ui;
}

void StakeForCharityDialog::setModel(WalletModel *model)
{
	this->model = model;
}

void StakeForCharityDialog::setAddress(const QString &address)
{
	setAddress(address, ui->charityAddressEdit);
}

void StakeForCharityDialog::setAddress(const QString &address, QLineEdit *addrEdit)
{
	addrEdit->setText(address);
	addrEdit->setFocus();
}

void StakeForCharityDialog::on_addressBookButton_clicked()
{
    if (model && model->getAddressTableModel())
    {
        AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::SendingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec())
			setAddress(dlg.getReturnValue(), ui->charityAddressEdit);
    }
}

void StakeForCharityDialog::on_viewButton_clicked()
{
	std::pair<std::string, int> pMultiSend;
	std::string strMultiSendPrint = "";
	if(pwalletMain->fMultiSend)
		strMultiSendPrint += "MultiSend Active\n";
	else
		strMultiSendPrint += "MultiSend Not Active\n";
	
	for(int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++)
	{
		pMultiSend = pwalletMain->vMultiSend[i];
		strMultiSendPrint += pMultiSend.first.c_str();
		strMultiSendPrint += " - ";
		strMultiSendPrint += boost::lexical_cast<string>(pMultiSend.second); 
		strMultiSendPrint += "% \n";
	}

	ui->message->setProperty("status", "ok");
    ui->message->style()->polish(ui->message);
    ui->message->setText(QString(strMultiSendPrint.c_str()));
    return;
}


void StakeForCharityDialog::on_addButton_clicked()
{
    bool fValidConversion = false;

    std::string strAddress = ui->charityAddressEdit->text().toStdString();
    if (!CBitcoinAddress(strAddress).IsValid())
    {
        ui->message->setProperty("status", "error");
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("The entered address:\n") + ui->charityAddressEdit->text() + tr(" is invalid.\nPlease check the address and try again."));
        ui->charityAddressEdit->setFocus();
        return;
    }

	
    int nCharityPercent = ui->charityPercentEdit->text().toInt(&fValidConversion, 10);
	int nSumMultiSend = 0;
	for(int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++)
		nSumMultiSend += pwalletMain->vMultiSend[i].second;

	if(nSumMultiSend + nCharityPercent > 100)
	{
		ui->message->setProperty("status", "error");
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("The total amount of your MultiSend vector is over 100% of your stake reward\n"));
        ui->charityAddressEdit->setFocus();
        return;
	}
	if (!fValidConversion || nCharityPercent > 100 || nCharityPercent <= 0)
    {
        ui->message->setProperty("status", "error");
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("Please Enter 1 - 100 for percent."));
        ui->charityPercentEdit->setFocus();
        return;
    }

	std::pair<std::string, int> pMultiSend;
	pMultiSend.first = strAddress;
	pMultiSend.second = nCharityPercent;
	pwalletMain->vMultiSend.push_back(pMultiSend);
	
    ui->message->setProperty("status", "ok");
    ui->message->style()->polish(ui->message);
	
	std::string strMultiSendPrint = "";
	for(int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++)
	{
		pMultiSend = pwalletMain->vMultiSend[i];
		strMultiSendPrint += pMultiSend.first.c_str();
		strMultiSendPrint += " - ";
		strMultiSendPrint += boost::lexical_cast<string>(pMultiSend.second); 
		strMultiSendPrint += "% \n";
	}
	CWalletDB walletdb(pwalletMain->strWalletFile);
	walletdb.WriteMultiSend(pwalletMain->vMultiSend);
    ui->message->setText(tr("MultiSend Vector\n") + QString(strMultiSendPrint.c_str()));
    return;
}

void StakeForCharityDialog::on_deleteButton_clicked()
{
    std::vector<std::pair<std::string, int> > vMultiSendTemp = pwalletMain->vMultiSend;
	std::string strAddress = ui->charityAddressEdit->text().toStdString();
	bool fRemoved = false;
	for(int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++)
	{
		if(pwalletMain->vMultiSend[i].first == strAddress)
		{
			pwalletMain->vMultiSend.erase(pwalletMain->vMultiSend.begin() + i);
			fRemoved = true;
		}
	}
	
	CWalletDB walletdb(pwalletMain->strWalletFile);
	
	if(!walletdb.EraseMultiSend(vMultiSendTemp))
		fRemoved = false;
	if(!walletdb.WriteMultiSend(pwalletMain->vMultiSend))
		fRemoved = false;
	
	if(fRemoved)
		ui->message->setText(tr("Removed ") + QString(strAddress.c_str()));
	else
		ui->message->setText(tr("Could not locate address\n"));
    return;
}

void StakeForCharityDialog::on_activateButton_clicked()
{
	std::string strRet = "";
	if(pwalletMain->vMultiSend.size() < 1)
		strRet = "Unable to activate MultiSend, check MultiSend vector\n";
	else if(CBitcoinAddress(pwalletMain->vMultiSend[0].first).IsValid())
	{
		pwalletMain->fMultiSend = true;
		CWalletDB walletdb(pwalletMain->strWalletFile);
		if(!walletdb.WriteMSettings(true, pwalletMain->nLastMultiSendHeight))
			strRet = "MultiSend activated but writing settings to DB failed";
		else
			strRet = "MultiSend activated";
	}
	else
		strRet = "First Address Not Valid";
	
    ui->message->setProperty("status", "ok");
    ui->message->style()->polish(ui->message);
    ui->message->setText(tr(strRet.c_str()));
    return;
}

void StakeForCharityDialog::on_disableButton_clicked()
{
	std::string strRet = "";
	pwalletMain->fMultiSend = false;
	CWalletDB walletdb(pwalletMain->strWalletFile);
	if(!walletdb.WriteMSettings(false, pwalletMain->nLastMultiSendHeight))
		strRet = "MultiSend deactivated but writing settings to DB failed";
	else
		strRet = "MultiSend deactivated";
	
    ui->message->setProperty("status", "");
    ui->message->style()->polish(ui->message);
    ui->message->setText(tr(strRet.c_str()));
    return;
}
