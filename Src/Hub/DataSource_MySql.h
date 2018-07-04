#ifndef PERSISTER_MYSQL_H
#define PERSISTER_MYSQL_H

#include <mysql.h>

#define REOPEN_DB_EVERY_TIME  (1)

namespace MarketHub
{

class DataSource_MySql : public DataSource
{
public:
	DataSource_MySql(ControllerImpl* controller);
	bool onStart();
    // Latest data at the front(rows[0]).
	int  retrieveData(const char*instrument, int resolution, int interval, int num, const DateTime& end, vector<FutureMarketData>& rows);
	bool onStop();

private:
	const char* getName() const;
	void parseConfig(const char* file);
	MYSQL* openMySqlConn(const char* database);
    int  retrieveBars(const char*instrument, int resolution, int interval, int num, const DateTime& end, vector<FutureMarketData>& rows);
    int  retrieveTicks(const char*instrument, int num, const DateTime& end, vector<FutureMarketData>& rows);
	int  loadHistoricalData(MYSQL* conn, const char*instrument, int resolution, int interval, int num, const DateTime& end, vector<FutureMarketData>& rows);
	void closeMySqlConn(MYSQL* conn);

private:
	const char* BARS_DB_NAME = "ctp_kbar";
    const char* TICKS_DB_NAME = "download_ctp_tick";

	typedef struct {
		char server[32];
		int  port;
		char user[32];
		char passwd[32];
	} MySQLContext;

#if !REOPEN_DB_EVERY_TIME
    MYSQL* m_conn;
#endif

	MySQLContext m_mySqlCtx;

	ControllerImpl* m_controller;
};

} // namespace MarketHub

#endif // PERSISTER_MYSQL_H