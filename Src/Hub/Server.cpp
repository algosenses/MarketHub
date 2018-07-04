#include "../util-internal.h"

#include <random>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include "config.h"
#include "ControllerImpl.h"
#include "Logger.h"
#include "Server.h"

using namespace rapidjson;
using namespace MarketHub;

/* Callback used for the /dump URI, and for every non-GET request:
 * dumps all information to stdout and gives back a trivial 200 ok */
void Server::dump_request_cb(struct evhttp_request *req, void *arg)
{
	assert(arg != nullptr);
	Server* srv = (Server*)arg;
	srv->dump_request(req);
}

void Server::dump_request(struct evhttp_request *req)
{
	const char *cmdtype;
	struct evkeyvalq *headers;
	struct evkeyval *header;
	struct evbuffer *buf;

	switch (evhttp_request_get_command(req)) {
	case EVHTTP_REQ_GET: cmdtype = "GET"; break;
	case EVHTTP_REQ_POST: cmdtype = "POST"; break;
	case EVHTTP_REQ_HEAD: cmdtype = "HEAD"; break;
	case EVHTTP_REQ_PUT: cmdtype = "PUT"; break;
	case EVHTTP_REQ_DELETE: cmdtype = "DELETE"; break;
	case EVHTTP_REQ_OPTIONS: cmdtype = "OPTIONS"; break;
	case EVHTTP_REQ_TRACE: cmdtype = "TRACE"; break;
	case EVHTTP_REQ_CONNECT: cmdtype = "CONNECT"; break;
	case EVHTTP_REQ_PATCH: cmdtype = "PATCH"; break;
	default: cmdtype = "unknown"; break;
	}

	printf("Received a %s request for %s\nHeaders:\n",
		cmdtype, evhttp_request_get_uri(req));

	headers = evhttp_request_get_input_headers(req);
	for (header = headers->tqh_first; header;
		header = header->next.tqe_next) {
		printf("  %s: %s\n", header->key, header->value);
	}

	buf = evhttp_request_get_input_buffer(req);
	puts("Input data: <<<");
	while (evbuffer_get_length(buf)) {
		int n;
		char cbuf[128];
		n = evbuffer_remove(buf, cbuf, sizeof(cbuf));
		if (n > 0)
			(void) fwrite(cbuf, 1, n, stdout);
	}
	puts(">>>");

	evhttp_send_reply(req, 200, "OK", NULL);
}

/* This callback gets invoked when we get any http request that doesn't match
 * any other callback.  Like any evhttp server callback, it has a simple job:
 * it must eventually call evhttp_send_error() or evhttp_send_reply().
 */
void Server::retrieve_data_cb(struct evhttp_request *req, void *arg)
{
	assert(arg != nullptr);
	Server* srv = (Server*)arg;
	srv->retrieve_data(req);
}

void Server::server_ctrl_cb(evhttp_request * req, void * arg)
{
	assert(arg != nullptr);
	Server* srv = (Server*)arg;

	struct evbuffer *evb = NULL;
	const char *uri = evhttp_request_get_uri(req);
	struct evhttp_uri *decoded = NULL;

	if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
		return;
	}

	decoded = evhttp_uri_parse(uri);
	if (!decoded) {
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
		return;
	}

	struct evkeyvalq http_query;
	evhttp_parse_query(uri, &http_query);
	const char* cmd = evhttp_find_header(&http_query, "cmd");
	const char* authCode = evhttp_find_header(&http_query, "auth");

	if (srv->m_srvAuthCode == atoi(authCode)) {
		if (!strcmp(cmd, "exit")) {
			srv->exit_server(req);
		}
	}
	
	evhttp_clear_headers(&http_query);

	if (decoded)
		evhttp_uri_free(decoded);

	return;
}

void Server::retrieve_data(struct evhttp_request *req)
{
	struct evbuffer *evb = NULL;
	const char *uri = evhttp_request_get_uri(req);
	struct evhttp_uri *decoded = NULL;
	const char *path;
	char *decoded_path;
	char *whole_path = NULL;
	size_t count;
	std::vector<string> rows;

	if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
		dump_request(req);
		return;
	}

	/* Decode the URI */
	decoded = evhttp_uri_parse(uri);
	if (!decoded) {
		printf("It's not a good URI. Sending BADREQUEST\n");
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
		return;
	}

	// http://localhost:5555/database?instrument=rb1705&interval=1&resolution=MINUTE&number=2000
	struct evkeyvalq http_query;
	evhttp_parse_query(uri, &http_query);
	const char* instrument = evhttp_find_header(&http_query, "instrument");
	const char* resolution = evhttp_find_header(&http_query, "resolution");
	const char* interval = evhttp_find_header(&http_query, "interval");
	const char* number = evhttp_find_header(&http_query, "number");

	StringBuffer s;
	Writer<StringBuffer> writer(s);

	/* Let's see what path the user asked for. */
	path = evhttp_uri_get_path(decoded);
	if (!path) path = "/";

	/* We need to decode it, to see what path the user really wanted. */
	decoded_path = evhttp_uridecode(path, 0, NULL);
	if (decoded_path == NULL)
		goto err;

	/* This holds the content we're sending. */
	evb = evbuffer_new();

	if (!instrument || !resolution || !interval || !number) {
		writer.StartObject();					
		writer.Key("result");					
		writer.Int(-1);		
		writer.Key("instrument");
		writer.String(instrument ? instrument : "NA");

		writer.Key("resolution");
		writer.String("NA");

		writer.Key("interval");
		writer.Int(0);
		writer.Key("number");
		writer.Int(0);
		writer.EndObject();

	} else {
		int res = Resolution::UNKNOWN;
		const char* resName = "UNKNOWN";
		if (!_stricmp(resolution, "TICK")) {
			res = Resolution::TICK;
			resName = "TICK";
		} else if (!_stricmp(resolution, "SECOND")) {
			res = Resolution::SECOND;
			resName = "SECOND";
		} else if (!_stricmp(resolution, "MINUTE")) {
			res = Resolution::MINUTE;
			resName = "MINUTE";
		} else if (!_stricmp(resolution, "HOUR")) {
			res = Resolution::HOUR;
			resName = "HOUR";
		} else if (!_stricmp(resolution, "DAY")) {
			res = Resolution::DAY;
			resName = "DAY";
		}

		if (instrument && instrument[0] != '\0' && res != Resolution::UNKNOWN && atoi(interval) > 0 && atoi(number) > 0) {
			m_controller->retrieveData(instrument, res, atoi(interval), atoi(number), rows);
		}

		count = rows.size();

		// { "result":0, "instrument":"rb1705", "resolution":"MINUTE", "interval":1, "number":2000 }
		writer.StartObject();                   // Between StartObject()/EndObject(), 
		writer.Key("result");					// output a key,
		writer.Int(count > 0 ? 0 : -1);			// follow by a value.
		writer.Key("instrument");
		writer.String(instrument ? instrument : "NA");

		writer.Key("resolution");
		writer.String(resName);

		writer.Key("interval");
		writer.Int(interval ? atoi(interval) : 0);
		writer.Key("number");
		writer.Int(count);
		if (count > 0) {
			writer.Key("data");
			writer.StartArray();					 // Between StartArray()/EndArray(),
			for (size_t i = 0; i < count; i++)
				writer.String(rows[i].c_str());      // all values are elements of the array.
			writer.EndArray();
		}
		writer.EndObject();
	}

	evbuffer_add_printf(evb, "%s", s.GetString());

	evhttp_add_header(evhttp_request_get_output_headers(req),
		"Content-Type", "text/html");

	evhttp_send_reply(req, 200, "OK", evb);
	goto done;

err:
	evhttp_send_error(req, 404, "Document was not found");

done:
	evhttp_clear_headers(&http_query);

	if (decoded)
		evhttp_uri_free(decoded);
	if (decoded_path)
		free(decoded_path);
	if (whole_path)
		free(whole_path);
	if (evb)
		evbuffer_free(evb);
}

void Server::exit_server(evhttp_request * req)
{
	event_base_loopbreak(m_evBase);
}

Server::Server(ControllerImpl* controller)
	: m_dispatcher(nullptr)
	, m_controller(controller)
	, m_running(false)
	, m_srvPort(5555)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
}

Server::~Server()
{
	WSACleanup();
}

void Server::dispatch()
{
	struct evhttp *http;
	struct evhttp_bound_socket *handle;

	m_evBase = event_base_new();
	if (!m_evBase) {
		Logger::instance()->sendLog(LOG_ERROR, "Couldn't create an event_base, exiting...");
		return;
	}

	/* Create a new evhttp object to handle requests. */
	http = evhttp_new(m_evBase);
	if (!http) {
		Logger::instance()->sendLog(LOG_ERROR, "couldn't create evhttp, exiting...");
		return;
	}

	/* The /dump URI will dump all requests to stdout and say 200 ok. */
	evhttp_set_cb(http, "/dump", dump_request_cb, this);
	evhttp_set_cb(http, "/control", server_ctrl_cb, this);
	evhttp_set_cb(http, "/database", retrieve_data_cb, this);

	/* Now we tell the evhttp what port to listen on */
	handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", m_srvPort);
	if (!handle) {
		Logger::instance()->sendLog(LOG_ERROR, "couldn't bind to port %d, exiting...", m_srvPort);
		return;
	}

	event_base_dispatch(m_evBase);

	evhttp_free(http);
	event_base_free(m_evBase);

	m_running.store(false);

	return;
}

bool Server::start()
{
	if (m_running.load()) {
		return true;
	}

	rude::Config rudeConfig;
	if (rudeConfig.load("MarketHub.ini")) {
		rudeConfig.setSection("Server");
		m_srvPort = rudeConfig.getIntValue("Port");
	}

	m_dispatcher = new std::thread(std::bind(&Server::dispatch, this));

	m_running.store(true);

	m_controller->sendLogMsg(LOG_INFO, "Start network server at port: %d.", m_srvPort);

	return true;
}

static void http_request_done(struct evhttp_request *req, void *arg) {
	// terminate event_base_dispatch()
//	event_base_loopbreak((struct event_base *)arg);
	event_base_loopexit((struct event_base *)arg, NULL);
}

void Server::stop()
{
	if (!m_running.load()) {
		return;
	}

	struct event_base *base = NULL;
	struct evhttp_connection *conn = NULL;
	struct evhttp_request *req = NULL;
	struct evkeyvalq *output_headers = NULL;

	base = event_base_new();
	conn = evhttp_connection_base_new(base, NULL, "127.0.0.1", m_srvPort);
	req = evhttp_request_new(http_request_done, base);

	output_headers = evhttp_request_get_output_headers(req);
	evhttp_add_header(output_headers, "Host", "localhost");
	evhttp_add_header(output_headers, "Connection", "close");

	char uri[128] = { 0 };
	std::random_device rd;
	std::mt19937 mt(rd());
	m_srvAuthCode = mt();
	sprintf(uri, "/control?cmd=exit&auth=%d", m_srvAuthCode);

	evhttp_make_request(conn, req, EVHTTP_REQ_GET, uri);
	evhttp_connection_set_timeout(conn, 600);
	event_base_dispatch(base);

	// Pretty sure the request should be freed automatically after the http_request_done 
	// has finished executing since you didn't use evhttp_request_own to take ownership
	// of the request object so it results in a double free.
	if (req) {
//		evhttp_request_free(req);
	}

	if (conn) {
		evhttp_connection_free(conn);
	}

	event_base_free(base);

	m_dispatcher->join();

	delete m_dispatcher;
}