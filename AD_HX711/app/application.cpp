// --------------------------------------
// Capacity AD7747 sensor example based on  Scanneri2c_scanner (Sming version)
//

#include "../include/user_config.h"
#include <SmingCore/SmingCore.h>
#include <SmingCore/Timer.h>
#include <SmingCore/HX711.h>

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put you SSID and Password here
#define WIFI_PWD "PleaseEnterPass"
#endif

Timer procTimer;

HttpServer server;
int totalActiveSockets = 0;
float pf;
HX711 hx711 = HX711(4, 5);

void onIndex(HttpRequest &request, HttpResponse &response) {
	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	//vars["counter"] = String(counter);
	response.sendTemplate(tmpl); // this template object will be deleted automatically
}

void onFile(HttpRequest &request, HttpResponse &response) {
	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else {
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void wsConnected(WebSocket& socket) {
	totalActiveSockets++;

	// Notify everybody about new connection
	WebSocketsList &clients = server.getActiveWebSockets();
	for (int i = 0; i < clients.count(); i++) {
		clients[i].sendString("New friend arrived! Total: " + String(totalActiveSockets));
	}
}

void wsMessageReceived(WebSocket& socket, const String& message) {
	//Serial.printf("WebSocket message received:\r\n%s\r\n", message.c_str());
	char buf[22];
	dtostrf(pf, 10, 8, buf);
	socket.sendString(buf);
}

void wsBinaryReceived(WebSocket& socket, uint8_t* data, size_t size) {
	Serial.printf("Websocket binary data receieved, size: %d\r\n", size);
}

void wsDisconnected(WebSocket& socket) {
	totalActiveSockets--;

	// Notify everybody about lost connection
	WebSocketsList &clients = server.getActiveWebSockets();
	for (int i = 0; i < clients.count(); i++)
		clients[i].sendString("We lost our friend :( Total: " + String(totalActiveSockets));
}

void readPeriodically() {
	while (!hx711.is_ready()) {
		delay(1);
	}
	long result = hx711.read();
	pf = result / 1.f;
	procTimer.initializeMs(150, readPeriodically).startOnce();
}

void startWebServer() {
	server.listen(80);
	server.addPath("/", onIndex);
	server.setDefaultHandler(onFile);

// Web Sockets configuration
	server.enableWebSockets(true);
	server.setWebSocketConnectionHandler(wsConnected);
	server.setWebSocketMessageHandler(wsMessageReceived);
	server.setWebSocketBinaryHandler(wsBinaryReceived);
	server.setWebSocketDisconnectionHandler(wsDisconnected);

	Serial.println("\r\n=== WEB SERVER STARTED ===");
	Serial.println(WifiStation.getIP());
	Serial.println("==============================\r\n");

}

// Will be called when WiFi station was connected to AP
void connectOk() {
	Serial.println("I'm CONNECTED");
	Serial.println(WifiStation.getIP().toString());
	startWebServer();
	readPeriodically();
}

void couldntConnect() {
	Serial.println("Couldn't connect");
}

void init() {
//debugf("Memory info: flashID %d size: %d\n",spi_flash_get_id(),fs_size());

	System.setCpuFrequency(eCF_160MHz);

	Serial.systemDebugOutput(false); // Disable debug output
	Serial.print("rst_info.reason: ");
	Serial.println(system_get_rst_info()->reason);
	Serial.print("rst_info.depc ");
	Serial.println(system_get_rst_info()->depc, HEX);
	Serial.print("rst_info.epc1 ");
	Serial.println(system_get_rst_info()->epc1, HEX);
	Serial.print("rst_info.epc2 ");
	Serial.println(system_get_rst_info()->epc2, HEX);
	Serial.print("rst_info.epc3 ");
	Serial.println(system_get_rst_info()->epc3, HEX);
	Serial.print("rst_info.exccause ");
	Serial.println(system_get_rst_info()->exccause, HEX);
	Serial.print("rst_info.excvaddr ");
	Serial.println(system_get_rst_info()->excvaddr, HEX);

	HX711 hx711;
	hx711.

	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiAccessPoint.enable(false);

// Run our method when station was connected to AP
	WifiStation.waitConnection(connectOk);
}

