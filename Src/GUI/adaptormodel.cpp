#include "adaptormodel.h"

AdaptorModel::AdaptorModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    m_adaptorStatus.clear();

    m_headerLabels << "ID";
    m_headerLabels << "Type";
    m_headerLabels << "Name";
    m_headerLabels << "Status";
}

AdaptorModel::~AdaptorModel()
{
}

QModelIndex AdaptorModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();
    return createIndex(row, column);
}

QModelIndex AdaptorModel::parent(const QModelIndex &index) const
{
    return QModelIndex();
}

QVariant AdaptorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    int indexColumn = index.column();
    int indexRow = index.row();
    if (indexRow < m_adaptorStatus.size()) {
        const AdaptorStatus& data = m_adaptorStatus[indexRow];
        switch (indexColumn) {
        case 0:
            return data.id;
        case 1:
            if (data.type == ADAPTOR_INPUT) {
                return QString("Input");
            } else if (data.type == ADAPTOR_OUTPUT) {
                return QString("Output");
            } else {
                return QString("Unknown");
            }
        case 2:
            return data.name;
        case 3:
            switch (data.status) {
            case ADAPTOR_LOAD:
                return QString("Load");
            case ADAPTOR_INIT:
                return QString("Init");
            case ADAPTOR_RUNNING:
                return QString("Running");
            case ADAPTOR_STOP:
                return QString("Stop");
            default:
                return QString("Unknown");
            }
            
        default:
            return QVariant();
        }
    }

    return QVariant();
}

/*
Qt::ItemFlags AdaptorModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}
*/

QVariant AdaptorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) return QVariant();
    if (role == Qt::TextAlignmentRole) return 0x0084;

    if (role!=Qt::DisplayRole) return QVariant();

    if (section >= m_headerLabels.count()) {
        return QVariant();
    }

    return m_headerLabels.at(section);
}

int AdaptorModel::rowCount(const QModelIndex &parent) const
{
    return m_adaptorStatus.size();
}

int AdaptorModel::columnCount(const QModelIndex &parent) const
{
    return m_headerLabels.count();
}

void AdaptorModel::clear()
{
}

void AdaptorModel::updateAdaptorStatus(const AdaptorStatus* pData)
{
    int i = 0;
    int size = m_adaptorStatus.size();
    for (i = 0; i < size; i++) {
        if (m_adaptorStatus[i].id == pData->id) {
            break;
        }
    }

    if (i == size) {
        int currentIndex = size;
        beginInsertRows(QModelIndex(), currentIndex, currentIndex);
        m_adaptorStatus.push_back(*pData);
        endInsertRows();
        emit dataChanged(index(m_adaptorStatus.size()-1,0),index(m_adaptorStatus.size()-1,m_headerLabels.size()-1));
    } else {
        m_adaptorStatus[i] = *pData;
        emit dataChanged(index(i,0),index(i,m_headerLabels.size()-1));
    }
}