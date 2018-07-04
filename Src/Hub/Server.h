#ifndef MARKET_HUB_SERVER_H
#define MARKET_HUB_SERVER_H

#include <thread>
#include <atomic>

namespace MarketHub
{

class Server
{
public:
	Server(ControllerImpl* controller);
	~Server();
	bool start();
	void stop();

private:
	static void dump_request_cb(struct evhttp_request *req, void *arg);
	static void retrieve_data_cb(struct evhttp_request *req, void *arg);
	static void server_ctrl_cb(struct evhttp_request *req, void *arg);
	void dispatch();
	void dump_request(struct evhttp_request *req);
	void retrieve_data(struct evhttp_request *req);
	void exit_server(struct evhttp_request *req);

private:
	int m_srvPort;
	int m_srvAuthCode;
	struct event_base *m_evBase;

	std::atomic<bool> m_running;
	std::thread* m_dispatcher;

	ControllerImpl* m_controller;
};

} // namespace MarketHub

#endif // MARKET_HUB_SERVER_H