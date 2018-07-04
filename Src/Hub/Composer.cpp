#include "Composer.h"

namespace MarketHub
{

Composer::Composer()
{
}

Composer::~Composer()
{
}

bool Composer::init(const char* instrument, int resolution, int interval, const TradingHours& hours)
{
	m_instrument = instrument;
	m_enable = false;
	m_hasNewData = false;
    m_cumVolOfLastBar = 0;
	memset(&m_lastMarketData, 0, sizeof(FutureMarketData));

	if (hours.size() == 0) {
		return false;
	}

	for (size_t i = 0; i < hours.size(); i++) {
		if (!checkTimeFormat(hours[i].begin) ||
			!checkTimeFormat(hours[i].end)) {
			return false;
		}
	}

	TradingSeconds secSession;
	if (!covertTradingSec(secSession, hours)) {
		return false;
	}

	m_session = secSession;

	if (resolution != HOUR &&
		resolution != MINUTE &&
		resolution != SECOND) {
		return false;
	}

	m_resolution = resolution;
	m_interval = interval;

	if (resolution == HOUR) {
		m_slicePeriod = 60 * 60 * interval;
	}
	else if (resolution == MINUTE) {
		m_slicePeriod = 60 * interval;
	}
	else {
		m_slicePeriod = interval;
	}

	computeSessionParams();

	m_enable = true;

	return true;
}

int Composer::appendMarketData(const FutureMarketData& data)
{
	if (m_enable) {
		return composite(data);
	}

	return RETURN_ERROR;
}

bool Composer::hasNewCompositedData()
{
	return m_hasNewData;
}

FutureMarketData Composer::getCompositedData()
{
	return m_lastMarketData;
}

int Composer::getInterval() const
{
	return m_interval;
}

int Composer::getResolution() const
{
	return m_resolution;
}

void Composer::computeSessionParams()
{
	m_totalTradableSecs = 0;
	for (size_t i = 0; i < m_session.size(); i++) {
		m_totalTradableSecs += (m_session[i].end - m_session[i].start + 1);
	}

	m_sliceTotalNum = m_totalTradableSecs / m_slicePeriod;
	m_currSliceIdx = INVALID_SLICE_INDEX;
}

int Composer::getSliceIndex(int currSecs)
{
	// index start from 0
	int idx = -1;
	int prevSliceCount = 0;

	for (size_t i = 0; i < m_session.size(); i++) {
		if (currSecs >= m_session[i].start && currSecs < m_session[i].end) {
			idx = (currSecs - m_session[i].start) / m_slicePeriod;
			idx += prevSliceCount;
		}
		prevSliceCount += (m_session[i].end - m_session[i].start) / m_slicePeriod;
	}

	return idx;
}

int Composer::getSliceEdgeTime(int sliceIdx)
{
    int preSlice = 0;
    for (size_t i = 0; i < m_session.size(); i++) {
        int currSlice = preSlice + (m_session[i].end - m_session[i].start) / m_slicePeriod;
        if (sliceIdx >= preSlice && sliceIdx < currSlice) {
            int second = m_session[i].start;
            second += (sliceIdx - preSlice) * m_slicePeriod;
            return (second / 3600) * 10000 + (second % 3600 / 60) * 100 + (second % 60);
        }
        preSlice = currSlice;
    }

    return 0;
}

void Composer::copyLastMarketData(bool completed, int sliceEdgeTime)
{
	memset(&m_lastMarketData, 0, sizeof(FutureMarketData));
	strcpy(m_lastMarketData.instrument, m_instrument.c_str());
	m_lastMarketData.tradingDay = m_lastInputDate;
    if (sliceEdgeTime != 0) {
        m_lastMarketData.updateTime = sliceEdgeTime;
    } else {
        m_lastMarketData.updateTime = m_lastInputTime;
    }
	m_lastMarketData.updateMillisec = m_lastInputMs;
	m_lastMarketData.openPrice = m_lastOpen;
	m_lastMarketData.highestPrice = m_lastHigh;
	m_lastMarketData.lowestPrice = m_lastLow;
	m_lastMarketData.closePrice = m_lastClose;
	m_lastMarketData.volume = m_lastVolume;
	m_lastMarketData.openInterest = m_lastOpenInt;

    if (completed) {
        m_lastMarketData.completed = true;
    } else {
        m_lastMarketData.completed = false;
    }
}

int Composer::composite(const FutureMarketData& data)
{
	m_hasNewData = false;

    if (m_cumVolOfLastBar == 0) {
        m_cumVolOfLastBar = data.volume;
    }

	int hour = data.updateTime / 10000;
	int min = (data.updateTime % 10000) / 100;
	int sec = data.updateTime % 100;
	int secs = hour * 3600 + min * 60 + sec;

	if (m_checkTimeline) {
		if (secs < m_lastInputBarSec) {
			return RETURN_ERROR;
		}
	}

	int sliceIdx = getSliceIndex(secs);
	if (sliceIdx <= INVALID_SLICE_INDEX) {
		return RETURN_ERROR;
	}

	if (m_checkTimeline) {
		if (sliceIdx < m_currSliceIdx) {
			return RETURN_ERROR;
		}
	}

	double price = data.lastPrice;

	if (m_currSliceIdx == INVALID_SLICE_INDEX) {
		m_currSliceIdx = sliceIdx;
		m_lastOpen = price;
		m_lastHigh = price;
		m_lastLow = price;
		m_lastOpen = price;
		m_lastVolume = data.volume - m_cumVolOfLastBar;
		m_lastOpenInt = data.openInterest;
		m_lastInputDate = data.tradingDay;
		m_lastInputTime = data.updateTime;
		m_lastInputMs = data.updateMillisec;

        m_cumVolOfLastBar = data.volume;

        copyLastMarketData();
	} else {
		m_hasNewData = false;
		if (m_checkTimeline) {
			if (sliceIdx > m_currSliceIdx) {
				m_hasNewData = true;
			}
		} else if (sliceIdx != m_currSliceIdx) {
			m_hasNewData = true;
		}

		if (m_hasNewData) {

            int sliceEdgeTime = getSliceEdgeTime(m_currSliceIdx);
			copyLastMarketData(true, sliceEdgeTime);

			m_lastOpen = price;
			m_lastHigh = price;
			m_lastLow = price;
			m_lastOpen = price;
			m_lastVolume = data.volume - m_cumVolOfLastBar;
			m_lastOpenInt = data.openInterest;

            m_cumVolOfLastBar = data.volume;

		} else {
			if (price > m_lastHigh) {
				m_lastHigh = price;
			}

			if (price < m_lastLow) {
				m_lastLow = price;
			}
			m_lastClose = price;
			m_lastVolume = data.volume - m_cumVolOfLastBar;
			m_lastOpenInt = data.openInterest;
			m_lastInputDate = data.tradingDay;
			m_lastInputTime = data.updateTime;
			m_lastInputMs = data.updateMillisec;

			copyLastMarketData();
		}
	}

	m_currSliceIdx = sliceIdx;
	m_lastInputBarSec = secs;
	m_lastInputDate = data.tradingDay;
	m_lastInputTime = data.updateTime;
	m_lastInputMs = data.updateMillisec;

	if (m_hasNewData) {
		return RETURN_COMPLETED_DATA;
	} else {
		return RETURN_UNCOMPLETED_DATA;
	}
}

bool Composer::checkTimeFormat(const string& time)
{
	if (time.length() != strlen("hh:mm:ss")) {
		return false;
	}

	char temp[16];
	memset(temp, 0, sizeof(temp));
	strcpy(temp, time.c_str());
	if (temp[2] != ':' || temp[5] != ':') {
		return false;
	}

	temp[2] = '\0';
	temp[5] = '\0';

	int d = atoi(temp);
	if (d < 0 || d > 23) {
		return false;
	}

	d = atoi(temp + 3);
	if (d < 0 || d > 59) {
		return false;
	}

	d = atoi(temp + 6);
	if (d < 0 || d > 59) {
		return false;
	}

	return true;
}

int Composer::str2sec(const string& time)
{
	char hourStr[3] = { 0 };
	char minStr[3] = { 0 };
	char secStr[3] = { 0 };

	const char *p = time.c_str();
	memcpy(hourStr, p, 2);
	p += 3;
	memcpy(minStr, p, 2);
	p += 3;
	memcpy(secStr, p, 2);

	return atoi(hourStr) * 3600 + atoi(minStr) * 60 + atoi(secStr);
}

bool Composer::covertTradingSec(TradingSeconds& secSessions, const TradingHours& session)
{
	if (session.size() == 0) {
		return false;
	}

	for (size_t i = 0; i < session.size(); i++) {
		TradingSecond secItem;

		int start = str2sec(session[i].begin);
		int end = str2sec(session[i].end);
		// split into two periods.
		if (end < start) {
			secItem.start = start;
			secItem.end = str2sec("24:00:00");
			secSessions.push_back(secItem);

			secItem.start = str2sec("00:00:00");
			secItem.end = end;
			secSessions.push_back(secItem);
		} else {
			secItem.start = start;
			secItem.end = end;
			secSessions.push_back(secItem);
		}
	}

	return true;
}

} // namespace MarketHub