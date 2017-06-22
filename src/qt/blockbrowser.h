#ifndef BLOCKBROWSER_H
#define BLOCKBROWSER_H

#include "clientmodel.h"
#include "main.h"
#include <QDialog>

namespace Ui {
class BlockBrowser;
}
class ClientModel;

class BlockBrowser : public QDialog
{
    Q_OBJECT

public:
    explicit BlockBrowser(QWidget *parent = 0);
    ~BlockBrowser();
    
    void setTransactionId(const QString &transactionId);
    void setModel(ClientModel *model);
    
public slots:
    
    void blockClicked();
    void txClicked();
    void updateExplorer(bool);
    double getTxFees(std::string);

private slots:

private:
    Ui::BlockBrowser *ui;
    ClientModel *model;
    
};

double getTxTotalValue(std::string); 
double getMoneySupply(qint64 Height);
double convertCoins(qint64); 
qint64 getBlockTime(qint64); 
qint64 getBlocknBits(qint64); 
qint64 getBlockNonce(qint64); 
qint64 getBlockHashrate(qint64); 
std::string getInputs(std::string); 
std::string getOutputs(std::string); 
std::string getBlockHash(qint64); 
std::string getBlockMerkle(qint64); 
bool addnode(std::string); 
const CBlockIndex* getBlockIndex(qint64);

#endif // BLOCKBROWSER_H
