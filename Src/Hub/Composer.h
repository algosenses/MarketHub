#ifndef MARKET_HUB_COMPOSER_H
#define MARKET_HUB_COMPOSER_H

#include <vector>
#include <string>
#include "MarketHub.h"

using std::vector;
using std::string;

namespace MarketHub
{

// Format: begin: "09:00:00", end: "10:15:00"
typedef struct {
    char begin[16];
    char end[16];
} TradingHour;

typedef vector<TradingHour> TradingHours;

typedef struct {
    long start;
    long end;
} TradingSecond;

typedef vector<TradingSecond> TradingSeconds;

enum Resolution {
    UNKNOWN,
    TICK,			 
    SECOND,				 
    MINUTE,			   
    HOUR,			
    DAY,		
    WEEK,	
};

class ComposerImpl;

class Composer {
public:
    enum {
        RETURN_COMPLETED_DATA,
        RETURN_UNCOMPLETED_DATA,
        RETURN_ERROR,
    };

    Composer();
    ~Composer();
    bool       init(const char* instrument, int resolution, int interval, const TradingHours& hours);
    int        appendMarketData(const FutureMarketData& data);
    bool       hasNewCompositedData();
    FutureMarketData getCompositedData();
    int        getInterval() const;
    int        getResolution() const;

private:
	bool checkTimeFormat(const string& time);
	int  str2sec(const string& time);
	bool covertTradingSec(TradingSeconds& secSessions, const TradingHours& session);
	void computeSessionParams();
	int  getSliceIndex(int currSecs);
    int  getSliceEdgeTime(int sliceIdx);
	void copyLastMarketData(bool completed = false, int sliceEdgeTime = 0);
	// 1: new data arrived.
	// 0: has not new data.
	// -1: do nothing.
	int composite(const FutureMarketData& data);

private:
	const int INVALID_SLICE_INDEX = -1;

	bool m_enable;
	string m_instrument;
	TradingSeconds m_session;
	int m_totalTradableSecs;
	int m_resolution;
	int m_interval;
	int m_slicePeriod;     // in seconds
	int m_sliceTotalNum;
	int m_currSliceIdx;
	bool m_checkTimeline;

	int       m_lastInputDate;
	int       m_lastInputTime;
	int       m_lastInputMs;
	int       m_lastInputBarSec;
	double    m_lastOpen;
	double    m_lastHigh;
	double    m_lastLow;
	double    m_lastClose;
	long long m_lastVolume;
	long long m_lastOpenInt;

	bool m_hasNewData;
	FutureMarketData m_lastMarketData;

    // Cumulate trading volume at the end of last bar.
    long long m_cumVolOfLastBar;
};

} // namespace MarketHub

#endif // MARKET_HUB_COMPOSER_H