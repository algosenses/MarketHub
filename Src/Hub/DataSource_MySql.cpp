#include <config.h>
#include "ControllerImpl.h"
#include "DataSource_MySql.h"

namespace MarketHub
{

DataSource_MySql::DataSource_MySql(ControllerImpl* controller)
	: m_controller(controller)
{
#if !REOPEN_DB_EVERY_TIME
	m_conn = NULL;
#endif
}

const char* DataSource_MySql::getName() const
{
	return "MySQL";
}

void DataSource_MySql::parseConfig(const char* file)
{
#if 1
	rude::Config rudeConfig;
	if (rudeConfig.load(file)) {
		rudeConfig.setSection("MySQL");
		const char* value = rudeConfig.getStringValue("IP");
		if (value && value[0] != '\0') {
			strncpy(m_mySqlCtx.server, value, sizeof(m_mySqlCtx.server));
		}

		m_mySqlCtx.port = rudeConfig.getIntValue("Port");

		value = rudeConfig.getStringValue("User");
		if (value && value[0] != '\0') {
			strncpy(m_mySqlCtx.user, value, sizeof(m_mySqlCtx.user));
		}

		value = rudeConfig.getStringValue("Password");
		if (value && value[0] != '\0') {
            char plaintext[128] = { 0 };
            if (Controller::decrypt(value, plaintext)) {
                strncpy(m_mySqlCtx.passwd, plaintext, sizeof(m_mySqlCtx.passwd));
            }
		}
	}
#else
	strcpy(m_mySqlCtx.server, "58.247.110.122");
	m_mySqlCtx.port = 61000;
	strcpy(m_mySqlCtx.user, "db_guest");
	strcpy(m_mySqlCtx.passwd, "Ats2016!@");
#endif
}

MYSQL* DataSource_MySql::openMySqlConn(const char * database)
{
	MYSQL* conn = mysql_init(NULL);
	if (!mysql_real_connect(
		conn,
		m_mySqlCtx.server,
		m_mySqlCtx.user,
		m_mySqlCtx.passwd,
		database,
		m_mySqlCtx.port,
		NULL,
		CLIENT_MULTI_RESULTS)) {
		m_controller->sendLogMsg(LOG_ERROR, "%s: MySQL %s", getName(), mysql_error(conn));
		return NULL;
	}
	else {
		m_controller->sendLogMsg(LOG_INFO, "%s: Connect to MySQL server [%s:%d] success.", getName(), m_mySqlCtx.server, m_mySqlCtx.port);
	}

	return conn;
}

int DataSource_MySql::loadHistoricalData(MYSQL* conn, const char*instrument, int resolution, int interval, int num, const DateTime& end, vector<FutureMarketData>& rows)
{
	char key[128] = { 0 };
	m_controller->getKey(key, instrument, interval, resolution);


	MYSQL_RES *res;
	MYSQL_ROW row;

	unsigned long tick = GetTickCount();

	char sql[1024] = { 0 };
	if (end.date == 0 || end.time == 0) {
		sprintf(sql, "SELECT * FROM %s.%s ORDER BY datetime DESC LIMIT %d", conn->db, instrument, num);
	} else {
		int year, month, day, hour, minute, second, ms;
		year = end.date / 10000; month = (end.date % 10000) / 100; day = end.date % 100;
		hour = end.time / 10000; minute = (end.time % 10000) / 100; second = end.time % 100;
		ms = end.ms;
		sprintf(sql, "SELECT * FROM %s.%s WHERE datetime < STR_TO_DATE('%4d-%02d-%02d %02d:%02d:%02d.%d', '%%Y-%%m-%%d %%H:%%i:%%s.%%f') ORDER BY datetime DESC LIMIT %d", 
			conn->db, instrument, year, month, day, hour, minute, second, ms*1000, num);
	}

	if (mysql_query(conn, sql)) {
		m_controller->sendLogMsg(LOG_ERROR, "%s: Load '%s' failed, %s", getName(), instrument, mysql_error(conn));
		return 0;
	} 

	tick = GetTickCount() - tick;

	res = mysql_use_result(conn);
	int fieldsNum = mysql_num_fields(res);
	vector<string> fieldNames;
	MYSQL_FIELD *fd;
	while (fd = mysql_fetch_field(res)) {
		fieldNames.push_back(fd->name);
	}

	int idx = 0;

	while ((row = mysql_fetch_row(res)) != NULL) {
		FutureMarketData data = { 0 };
		strcpy(data.instrument, instrument);
		for (int i = 0; i < fieldsNum; i++) {
			if (!_stricmp(fieldNames[i].c_str(), "Open")) {
				data.openPrice = atof(row[i]);
			} else if (!_stricmp(fieldNames[i].c_str(), "High")) {
				data.highestPrice = atof(row[i]);
			} else if (!_stricmp(fieldNames[i].c_str(), "Low")) {
				data.lowestPrice = atof(row[i]);
			} else if (!_stricmp(fieldNames[i].c_str(), "Close")) {
				data.closePrice = atof(row[i]);
			} else if (!_stricmp(fieldNames[i].c_str(), "Volume")) {
				data.volume = atol(row[i]);
			} else if (!_stricmp(fieldNames[i].c_str(), "OpenInt")) {
				data.openInterest = atol(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "upper_limit_price")) {
                data.upperLimitPrice = atof(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "lower_limit_price")) {
                data.lowerLimitPrice = atof(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "last_price")) {
                data.lastPrice = atof(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "open_int")) {
                data.openInterest = atof(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "ask_price1")) {
                data.askPrice1 = atof(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "ask_vol1")) {
                data.askVolume1 = atol(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "bid_price1")) {
                data.bidPrice1 = atof(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "bid_vol1")) {
                data.bidVolume1 = atol(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "pre_settle_price")) {
                data.preSettlePrice = atof(row[i]);
            } else if (!_stricmp(fieldNames[i].c_str(), "datetime")) {
				// Format: 2016-10-20 21:03:00.000000
				char dt[128];
				strcpy(dt, row[i]);
				dt[4] = '\0'; dt[7] = '\0'; dt[10] = '\0'; dt[13] = '\0'; dt[16] = '\0'; dt[19] = '\0';
				data.tradingDay = atoi(dt) * 10000 + atoi(dt + 5) * 100 + atoi(dt + 8);
				data.updateTime = atoi(dt + 11) * 10000 + atoi(dt + 14) * 100 + atoi(dt + 17);
				data.updateMillisec = atoi(dt + 20) / 1000;
			}
		}

        data.completed = true;
		rows.push_back(data);

		idx++;
	}
	mysql_free_result(res);

	m_controller->sendLogMsg(LOG_INFO, "%s: Load '%s', consume %ld ms, return %d rows.", getName(), key, tick, idx);

	return idx;
}

void DataSource_MySql::closeMySqlConn(MYSQL * conn)
{
	mysql_close(conn);
}

bool DataSource_MySql::onStart()
{
	parseConfig("MarketHub.ini");
#if !REOPEN_DB_EVERY_TIME
	m_conn = openMySqlConn(BARS_DB_NAME);
	return (m_conn != NULL);
#else
    return true;
#endif
}

int DataSource_MySql::retrieveBars(const char*instrument, int resolution, int interval, int num, const DateTime& end, vector<FutureMarketData>& rows)
{
    if (resolution != Resolution::MINUTE || interval != 1) {
        return 0;
    }

#if !REOPEN_DB_EVERY_TIME
    if (m_conn == NULL) {
        return 0;
    }
    return loadHistoricalData(m_conn, instrument, resolution, interval, num, end, rows);
#else
    MYSQL* conn = openMySqlConn(BARS_DB_NAME);
    int result = 0;
    if (conn) {
        result = loadHistoricalData(conn, instrument, resolution, interval, num, end, rows);
        closeMySqlConn(conn);
    }
    return result;
#endif
}

int DataSource_MySql::retrieveTicks(const char*instrument, int num, const DateTime& end, vector<FutureMarketData>& rows)
{
    MYSQL* conn = openMySqlConn(TICKS_DB_NAME);
    int result = 0;
    if (conn) {
        result = loadHistoricalData(conn, instrument, Resolution::TICK, 1, num, end, rows);
        closeMySqlConn(conn);
    }
    return result;
}

int DataSource_MySql::retrieveData(const char*instrument, int resolution, int interval, int num, const DateTime& end, vector<FutureMarketData>& rows)
{
    if (resolution == Resolution::TICK) {
        return retrieveTicks(instrument, num, end, rows);
    } else {
        return retrieveBars(instrument, resolution, interval, num, end, rows);
    }
}

bool DataSource_MySql::onStop()
{
#if !REOPEN_DB_EVERY_TIME
    if (m_conn) {
		closeMySqlConn(m_conn);
		m_conn = NULL;
	}
#endif

	return true;
}

} // namespace MarketHub