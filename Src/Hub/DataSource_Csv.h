#ifndef PERSISTER_CSV_H
#define PERSISTER_CSV_H

#define CSV_FIELD_NUM  (9)

namespace MarketHub
{

class DataSource_Csv : public DataSource
{
public:
	DataSource_Csv(ControllerImpl* control);
    int retrieveData(const char*instrument,
                     int resolution,
                     int interval, 
                     int num,
                     const DateTime& end,
                     std::vector<FutureMarketData>& rows);

private:
	int loadHistoricalData(const char* filename, vector<FutureMarketData>& data);
    int loadMultiChartsExportedCsv(const char* instrument, int resolution, int interval, int num, vector<FutureMarketData>& data);

private:
	ControllerImpl* m_control;

    std::string m_csvPath;
};

} // namespace MarketHub

#endif // PERSISTER_CSV_H