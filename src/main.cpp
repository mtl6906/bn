#include "ls/http/Request.h"
#include "ls/http/Response.h"
#include "ls/http/QueryString.h"
#include "ls/io/InputStream.h"
#include "ls/io/OutputStream.h"
#include "ls/ssl/Client.h"
#include "ls/net/Client.h"
#include "ls/json/API.h"
#include "ls/SHA256.h"
#include "ls/DefaultLogger.h"
#include "string"
#include "vector"
#include "iostream"
#include "memory"
#include "stack"
#include "unistd.h"

using namespace ls;
using namespace std;

char *ip, *url, *secretKey, *apiKey, *coin;
double rate, uprate;
double number;

string transacation(const string &method, const string &url, const string &body = "", const map<string, string> &attributes = map<string, string>())
{
	http::Request request;
	request.setDefaultHeader();
	request.getMethod() = method;
	request.getURL() = url;
	request.getBody() = body;
	request.getVersion() = "HTTP/1.1";
	request.setAttribute("Host", "api.binance.com");
	request.setAttribute("User-Agent", "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:89.0) Gecko/20100101 Firefox/89.0");
	if(body != "")
		request.setAttribute("Content-Length", to_string(body.size()));
	for(auto &attribute : attributes)
		request.setAttribute(attribute.first, attribute.second);
	ssl::Client sslClient;
	unique_ptr<ssl::Connection> connection(sslClient.getConnection(net::Client(ip, 443).connect()));
	connection -> setHostname(url);
	connection -> connect();
	
	io::OutputStream out(connection -> getWriter(), new Buffer());
	string text = request.toString();
	
	LOGGER(ls::INFO) << "request:\n" << text << ls::endl;
	
	out.append(text);
	out.write();

	LOGGER(ls::INFO) << "cmd sending..." << ls::endl;

	io::InputStream in(connection -> getReader(), new Buffer());


	http::Response response;
	for(;;)
	{
		in.read();
		LOGGER(ls::INFO) << "reading..." << ls::endl;
		if(response.getCode() == "")
		{
			try
			{
				string responseText = in.split("\r\n\r\n", true);
				response.parseFrom(responseText);
			}
			catch(Exception &e)
			{
				sleep(1);
				continue;	
			}
		}
		auto contentLength = stoi(response.getAttribute("Content-Length"));
		try
		{
			auto text = in.split(contentLength);
			return text;
		}
		catch(Exception &e)
		{
			
		}
		sleep(1);
	}
	return "";
}

vector<double> getPrice(const string &coin)
{
	vector<double> prices(2);
	string text = transacation("GET", string("/api/v3/ticker/bookTicker?symbol=") + coin);
	cout << text << endl;
	auto root = json::api.decode(text);
	string price;
	json::api.get(root, "bidPrice", price);
	prices[0] = stod(price);
	json::api.get(root, "askPrice", price);
	prices[1] = stod(price);
	return prices;
}

string order(const string &coin, double price, double number, const string &type)
{
	map<string, string> attribute;
	attribute["Content-Type"] = "application/x-www-form-urlencoded";
	attribute["X-MBX-APIKEY"] = apiKey;
	http::QueryString qs;
	qs.setParameter("symbol", coin);
	qs.setParameter("side", type);
	qs.setParameter("type", "LIMIT");
	qs.setParameter("timeInForce", "GTC");
	qs.setParameter("quantity", to_string(number));
	qs.setParameter("price", to_string(price));
	qs.setParameter("timestamp", to_string(time(NULL)*1000));
	qs.setParameter("recvWindow", to_string(5000));
	string body = qs.toString();
	ls::SHA256 sha256;
	string signature = sha256.hmac(body, secretKey);
	body.append("&signature=");
	body.append(signature);
	return transacation("POST", "/api/v3/order", body, attribute);
}

void buy(const string &coin, double price, double number)
{
	auto text = order(coin, price, number, "BUY");
	LOGGER(ls::INFO) << text << ls::endl;
}

void sell(const string &coin, double price, double number)
{
	auto text = order(coin, price, number, "SELL");
	LOGGER(ls::INFO) << text << ls::endl;
}

pair<int, double> getBuyOrderNumberAndMax(const string &coin)
{
	map<string, string> attribute;
	attribute["X-MBX-APIKEY"] = apiKey;
	int count = 0;
	http::QueryString qs;
	qs.setParameter("symbol", coin);
	qs.setParameter("recvWindow", "5000");
	qs.setParameter("timestamp", to_string(time(NULL) * 1000));
	string url = "/api/v3/openOrders?";
	auto text = qs.toString();
	url += text + "&signature=";
	ls::SHA256 sha256;
	url += sha256.hmac(text, secretKey);
	auto responseText = transacation("GET", url, "", attribute);

	LOGGER(ls::INFO) << responseText << ls::endl;

	double maxPrice = 0;
	json::Array array;
	array.parseFrom(responseText);
	for(int i=0;i<array.size();++i)
	{
		json::Object it;
		string type;
		json::api.get(array, i, it);
		json::api.get(it, "side", type);
		if(type == "BUY")
		{
			++count;
			string text;
			json::api.get(it, "price", text);
			double price = stod(text);
			int p1 = (int)(maxPrice * 100), p2 = (int)(price * 100);
			if(p1 < p2)
				maxPrice = price;
		}
	}
	pair<int, double> result;
	result.first = count;
	result.second = maxPrice;
	return result;
}

double round2(double value)
{
	int v = value * 100;
	return v / 100.0;
}

void method()
{
	for(;;)
	{
		sleep(2);
		auto prices = getPrice(coin);
		auto buyOrderNumber = getBuyOrderNumberAndMax(coin);
		if(buyOrderNumber.first == 0)
		{
			sell(coin, prices[0], number);
			buy(coin, round2(prices[0] * (1 - rate)), number);
		}
		else
		{
			if(buyOrderNumber.first >= 5)
				continue;
			long long currentPrice = (long long)(prices[0] * 10000);
			long long signPriceNow = (long long)(buyOrderNumber.second * (1 + uprate) * 10000);
			if(currentPrice > signPriceNow)
			{
				sell(coin, prices[0], number);
				buy(coin, round2(prices[0] * (1 - rate)), number);
			}
		}
	}
}

int main(int argc, char **argv)
{
	ip = argv[1];      
	url = argv[2];
	apiKey = argv[3];
	secretKey = argv[4];
	rate = stod(argv[5]);
	uprate = stod(argv[6]);
	coin = argv[7];
	number = stod(argv[8]);
	LOGGER(ls::INFO) << "rate: " << rate << ls::endl;
//	getPrice();
//	cout << buy("GALAUSDT", 0.08, 200) << endl;
//	cout << sell("ARUSDT", 90, 0.3) << endl;
//	cout << getBuyOrderNumber("GALAUSDT") << endl;
	method();
}
