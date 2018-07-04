#include "csv_parser.hpp"
#include "ControllerImpl.h"
#include "DataSource_Csv.h"
#include "MarketHub.h"

namespace MarketHub
{

DataSource_Csv::DataSource_Csv(ControllerImpl* control)
	: m_control(control)
{
    m_csvPath = "./CSV";
}

int DataSource_Csv::retrieveData(const char*instrument,
    int resolution,
    int interval, 
    int num,
    const DateTime& end,
    std::vector<FutureMarketData>& rows)
{
    if (instrument == nullptr || instrument[0] == '\0') {
        return 0;
    }

    if (num <= 0) {
        return 0;
    }

    // only support load all data.
    if (end.date != 0 || end.time != 0 || end.ms != 0) {
        return 0;
    }

    return loadMultiChartsExportedCsv(instrument, resolution, interval, num, rows);
}

int DataSource_Csv::loadMultiChartsExportedCsv(const char* instrument, int resolution, int interval, int num, vector<FutureMarketData>& data)
{
    const char* res = NULL;
    switch (resolution) {
    case SECOND:
        res = "SECOND";
        break;
    case MINUTE:
        res = "MINUTE";
        break;
    default:
        return 0;
        break;
    }

    char filename[256];
    sprintf(filename, "%s/%s-%d-%s.csv", m_csvPath.c_str(), instrument, interval, res);

    /* Declare the variables to be used */
    const char field_terminator = ',';
    const char line_terminator = '\n';
    const char enclosure_char = '"';

    csv_parser* parser = new csv_parser();

    if (parser == nullptr) {
        return 0;
    }

    char log[128];
    sprintf(log, "Load csv file '%s'...", filename);
    m_control->sendLogMsg(LOG_INFO, log);

    /* Define how many records we're gonna skip. This could be used to skip the column definitions. */
    parser->set_skip_lines(1);

    /* Specify the file to parse */
    if (!parser->init(filename)) {
        delete parser;
        return 0;
    }

    /* Here we tell the parser how to parse the file */
    parser->set_enclosed_char(enclosure_char, ENCLOSURE_OPTIONAL);

    parser->set_field_term_char(field_terminator);

    parser->set_line_term_char(line_terminator);

    unsigned int row_count = 0U;

    std::vector<FutureMarketData> temp;

    /* Check to see if there are more records, then grab each row one at a time */
    while (parser->has_more_rows())
    {
        unsigned int i = 0;

        /* Get the record */
        csv_row row = parser->get_row();

        // Example:
        // <Date>, <Time>, <Open>, <High>, <Low>, <Close>, <Volume>
        const int filed_num = 7;
        if (row.size() == filed_num) {
            // date
            char d[32] = { 0 };
            strcpy(d, row[0].c_str());
            char* p = strchr(d, '/');
            if (p == NULL) {
                continue;
            }

            *p = '\0';
            p++;
            char* q = strchr(p, '/');
            if (q == NULL) {
                continue;
            }
            *q++ = '\0';

            int year = atoi(d);
            int month = atoi(p);
            int day = atoi(q);

            if (month > 12 || day > 31) {
                continue;
            }

            // time
            char t[32] = { 0 };
            strcpy(t, row[1].c_str());
            p = strchr(t, ':');
            if (p == nullptr) {
                continue;
            }

            *p = '\0';
            p++;
            q = strchr(p, ':');
            if (q == nullptr) {
                continue;;
            }
            *q++ = '\0';
            int hour, minute, sec;
            hour = atoi(t);
            minute = atoi(p);
            sec = atoi(q);

            if ((hour < 0 || hour > 23) || (minute < 0 || minute > 59) || (sec < 0 || sec >59)) {
                continue;
            }

            double open = atof(row[2].c_str());
            double high = atof(row[3].c_str());
            double low = atof(row[4].c_str());
            double close = atof(row[5].c_str());
            int volume = atoi(row[6].c_str());
            int openint = 0;

            try {
                FutureMarketData d = { 0 };
                strcpy(d.instrument, instrument);
                d.tradingDay = year * 10000 + month * 100 + day;
                d.updateTime = hour * 10000 + minute * 100 + sec;
                d.openPrice = open;
                d.highestPrice = high;
                d.lowestPrice = low;
                d.closePrice = close;
                d.openInterest = openint;
                d.volume = volume;
                d.completed = true;

                temp.push_back(d);
            }
            catch (std::invalid_argument e) {
                return false;
            }
        }

        row_count++;
    }

    delete parser;

    int size = temp.size();
    int count = 0;
    for (size_t i = 0; i < num && i < size; i++) {
        data.push_back(temp[size-i-1]);
        count++;
    }

    return count;
}

int DataSource_Csv::loadHistoricalData(const char* filename, vector<FutureMarketData>& data)
{
	char key[128] = { 0 };

	string path(filename);
	std::string basename = path.substr(path.find_last_of("/\\") + 1);
	basename = basename.substr(0, basename.rfind("."));
	strcpy(key, basename.c_str());

	/* Declare the variables to be used */
	const char field_terminator = ',';
	const char line_terminator = '\n';
	const char enclosure_char = '"';

	csv_parser* parser = new csv_parser();

	if (parser == nullptr) {
		return 0;
	}

	char log[128];
	sprintf(log, "Load csv file '%s'...", filename);
	m_control->sendLogMsg(LOG_INFO, log);

	/* Define how many records we're gonna skip. This could be used to skip the column definitions. */
	parser->set_skip_lines(1);

	/* Specify the file to parse */
	if (!parser->init(filename)) {
		delete parser;
		return 0;
	}

	/* Here we tell the parser how to parse the file */
	parser->set_enclosed_char(enclosure_char, ENCLOSURE_OPTIONAL);

	parser->set_field_term_char(field_terminator);

	parser->set_line_term_char(line_terminator);

	unsigned int row_count = 0U;

	/* Check to see if there are more records, then grab each row one at a time */
	while (parser->has_more_rows())
	{
		unsigned int i = 0;

		/* Get the record */
		csv_row row = parser->get_row();

		// instrument,date,time,open,high,low,close,volume,openint
		// Example:
		// rb1305,11/01/2012,9:00,3631.0,3638.0,3627.0,3629.0,58972,983882
		if (row.size() == CSV_FIELD_NUM) {
			// instrument
			string instrument = row[0];

			// date
			char d[32] = { 0 };
			strcpy(d, row[1].c_str());
			if (strlen(d) != strlen("yyyy/mm/dd")) {
				continue;
			}
			if (d[4] != '/' || d[7] != '/') {
				continue;
			}

			int year, month, day;
			d[4] = '\0';
			d[7] = '\0';
			year = atoi(d);
			month = atoi(d + 5);
			day = atoi(d + 8);

			if (month > 12 || day > 31) {
				continue;
			}

			// time
			char t[32] = { 0 };
			strcpy(t, row[2].c_str());
			char* p = strchr(t, ':');
			if (p == nullptr) {
				continue;
			}

			*p = '\0';
			p++;
			char* q = strchr(p, ':');
			if (q == nullptr) {
				continue;;
			}
			*q++ = '\0';
			int hour, minute, sec;
			hour = atoi(t);
			minute = atoi(p);
			sec = atoi(q);

			if ((hour < 0 || hour > 23) || (minute < 0 || minute > 59) || (sec < 0 || sec >59)) {
				continue;
			}


			double open = atof(row[3].c_str());
			double high = atof(row[4].c_str());
			double low = atof(row[5].c_str());
			double close = atof(row[6].c_str());
			int volume = atoi(row[7].c_str());
			int openint = atoi(row[8].c_str());

			try {
				FutureMarketData d = { 0 };
				strcpy(d.instrument, instrument.c_str());
				d.tradingDay = year * 10000 + month * 100 + day;
				d.updateTime = hour * 10000 + minute * 100 + sec;
				d.openPrice = open;
				d.highestPrice = high;
				d.lowestPrice = low;
				d.closePrice = close;
				d.openInterest = openint;
				d.volume = volume;

				data.push_back(d);
			} catch (std::invalid_argument e) {
				return false;
			}
		}

		row_count++;
	}

	delete parser;

	return row_count;
}

} // namespace MarketHub