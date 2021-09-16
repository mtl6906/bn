#include "ls/http/Request.h"
#include "ls/io/InputStream.h"
#include "ls/io/OutputStream.h"
#include "ls/ssl/Client.h"
#include "ls/net/Client.h"
#include "string"
#include "vector"
#include "iostream"
#include "memory"

using namespace ls;
using namespace std;

char *ip, *url;

string transacation(const string &method, const string &url, const string &body = "")
{
	http::Request request;
	request.setDefaultHeader();
	request.getMethod() = "GET";
	request.getURL() = "/api/v3/ticker/bookTicker?symbol=FTMUSDT";
	request.getVersion() = "HTTP/1.1";
	request.setAttribute("Host", "api.binance.com");
	request.setAttribute("User-Agent", "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:89.0) Gecko/20100101 Firefox/89.0");

	ssl::Client sslClient;
	unique_ptr<ssl::Connection> connection(sslClient.getConnection(net::Client(ip, 443).connect()));
	connection -> setHostname(url);
	connection -> connect();
	
	io::OutputStream out(connection -> getWriter(), new Buffer());
	out.append(request.toString());
	out.write();
	
	io::InputStream in(connection -> getReader(), new Buffer());
	in.read();
	return in.split();
}

void get

int main(int argc, char **argv)
{
	ip = argv[1];
	url = argv[1];
	cout << getPrice() << endl;
}
