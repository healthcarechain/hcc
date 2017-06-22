#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"

#include "main.h"
#include "bitcoinrpc.h"
#include "util.h"

double GetPoSKernelPS(const CBlockIndex* pindex);

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 64
#define NUM_ITEMS 4

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        painter->setBackground(QBrush(QColor(255,255,255),Qt::SolidPattern));
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = qvariant_cast<QColor>(value);
        }
foreground = QColor(51,51,51);
        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
            foreground = QColor(51,153,51);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->setPen(QColor(204,204,204));
        painter->drawLine(QPoint(mainRect.left(), mainRect.bottom()), QPoint(mainRect.right(), mainRect.bottom()));
        painter->drawLine(QPoint(amountRect.left()-6, mainRect.top()), QPoint(amountRect.left()-6, mainRect.bottom()));
        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    ui->setupUi(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

	
    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("Out of sync with the HealthCareChain network") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("Out of sync with the HealthCareChain network") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    if(GetBoolArg("-chart", true))
    {
        // setup Plot
        // create graph
        ui->diffplot->addGraph();

        // give the axes some labels:
        ui->diffplot->xAxis->setLabel("Block Height");
        ui->diffplot->yAxis->setLabel("24H Network Weight");

        // set the pens
        ui->diffplot->graph(0)->setPen(QPen(QColor(40, 110, 173)));  //ChipRed - RGB 204, 70, 62 - not looking nice, going with blue pen
        ui->diffplot->graph(0)->setLineStyle(QCPGraph::lsLine);

        // set axes label fonts:
        QFont label = font();
        ui->diffplot->xAxis->setLabelFont(label);
        ui->diffplot->yAxis->setLabelFont(label);
    }
    else
    {
        ui->diffplot->setVisible(false);
    }
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::updatePlot(int count)
{
    static int64_t lastUpdate = 0;
    // Double Check to make sure we don't try to update the plot when it is disabled
    if(!GetBoolArg("-chart", true)) { return; }
    if (GetTime() - lastUpdate < 180) { return; } // This is just so it doesn't redraw rapidly during syncing

    if(fDebug) { printf("Plot: Getting Ready: pindexBest: %p\n", pindexBest); }
    	
		bool fProofOfStake = (nBestHeight > LAST_POW_BLOCK + 100);
    if (fProofOfStake)
        ui->diffplot->yAxis->setLabel("24H Network Weight");
		else
        ui->diffplot->yAxis->setLabel("Difficulty");

    int numLookBack = 1440;
    double diffMax = 0;
    const CBlockIndex* pindex = GetLastBlockIndex(pindexBest, fProofOfStake);
    int height = pindex->nHeight;
    int xStart = std::max<int>(height-numLookBack, 0) + 1;
    int xEnd = height;

    // Start at the end and walk backwards
    int i = numLookBack-1;
    int x = xEnd;

    // This should be a noop if the size is already 2000
    vX.resize(numLookBack);
    vY.resize(numLookBack);

    if(fDebug) {
        if(height != pindex->nHeight) {
            printf("Plot: Warning: nBestHeight and pindexBest->nHeight don't match: %d:%d:\n", height, pindex->nHeight);
        }
    }

    if(fDebug) { printf("Plot: Reading blockchain\n"); }

    const CBlockIndex* itr = pindex;
    while(i >= 0 && itr != NULL)
    {
        if(fDebug) { printf("Plot: Processing block: %d - pprev: %p\n", itr->nHeight, itr->pprev); }
        vX[i] = itr->nHeight;
        if (itr->nHeight < xStart) {
        	xStart = itr->nHeight;
        }
        vY[i] = fProofOfStake ? GetPoSKernelPS(itr) : GetDifficulty(itr);
        diffMax = std::max<double>(diffMax, vY[i]);

        itr = GetLastBlockIndex(itr->pprev, fProofOfStake);
        i--;
        x--;
    }

    if(fDebug) { printf("Plot: Drawing plot\n"); }

    ui->diffplot->graph(0)->setData(vX, vY);

    // set axes ranges, so we see all data:
    ui->diffplot->xAxis->setRange((double)xStart, (double)xEnd);
    ui->diffplot->yAxis->setRange(0, diffMax+(diffMax/10));

    ui->diffplot->xAxis->setAutoSubTicks(false);
    ui->diffplot->yAxis->setAutoSubTicks(false);
    ui->diffplot->xAxis->setSubTickCount(0);
    ui->diffplot->yAxis->setSubTickCount(0);

    ui->diffplot->replot();

    if(fDebug) { printf("Plot: Done!\n"); }
    	
    lastUpdate = GetTime();
}
 

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->labelStake->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

/*
void OverviewPage::setStrength(double strength)
{
    QString strFormat;
    if (strength == 0)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 0;
    }
	 else if(strength < 0.01)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 1;
    }
	else if(strength < 0.02)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 2;
    }
	else if(strength < 0.03)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 3;
    }
	else if(strength < 0.04)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 4;
    }
	 else if(strength < 0.05)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 5;
    }
	else if(strength < 0.06)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 6;
    }
	else if(strength < 0.07)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 7;
    }
	else if(strength < 0.08)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 8;
    }
	else if(strength < 0.09)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 9;
    }
    else if(strength < 0.1)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 10;
    }
	else if(strength < 0.11)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 11;
    }
	else if(strength < 0.12)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 12;
    }
	else if(strength < 0.13)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 13;
    }
	else if(strength < 0.14)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 14;
    }
	else if(strength < 0.15)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 15;
    }
	else if(strength < 0.16)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 1;
    }
	else if(strength < 0.16)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 17;
    }
	else if(strength < 0.18)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 18;
    }
	else if(strength < 0.19)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 19;
    }
    else if (strength < 0.2)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 20;
    }
	else if(strength < 0.25)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 25;
    }
    else if (strength < 0.3)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 25;
    }
	else if(strength < 0.35)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 35;
    }
    else if (strength < 0.4)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 40;
    }
	else if(strength < 0.45)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 45;
    }
    else if (strength < 0.5)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 50;
    }
	else if(strength < 0.55)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 55;
    }
    else if (strength < 0.6)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 60;
    }
	else if(strength < 0.65)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 65;
    }
    else if (strength < 0.7)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 70;
    }
	else if(strength < 0.75)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 75;
    }
    else if (strength < 0.8)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 80;
    }
    else if (strength < 0.9)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 90;
    }
    else if (strength <= 1.0)
    {
        strFormat = "My Estimated Network Weight";
        currentStrength = 100;
    }
    else
    {
        strFormat = "Error!";
    }
    ui->strengthBar->setValue(currentStrength);
    ui->strengthBar->setFormat(strFormat);
    ui->strengthBar->setTextVisible(true);
}
*/

void OverviewPage::setNumTransactions(int count)
{
    ui->labelNumTransactions->setText(QLocale::system().toString(count));
}

void OverviewPage::setModel(WalletModel *model)
{
    this->model = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("HCC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = model->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
