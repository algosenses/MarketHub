#ifndef ADAPTOR_MODEL_H
#define ADAPTOR_MODEL_H

#include <vector>
#include <QAbstractItemModel>
#include "MarketHub.h"

using namespace MarketHub;

class AdaptorModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    AdaptorModel(QObject *parent);
    ~AdaptorModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    //    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    void clear();
    void updateAdaptorStatus(const AdaptorStatus* pData);

private:
    QStringList m_headerLabels;
    std::vector<AdaptorStatus> m_adaptorStatus;
};

#endif // ADAPTOR_MODEL_H