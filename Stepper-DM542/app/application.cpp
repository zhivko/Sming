#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Wiring/SplitString.h>

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put you SSID and Password here
#define WIFI_PWD "PleaseEnterPass"
#endif

Timer procTimer;
Timer reportTimer;

#define UPDATE_PIN 0 // GPIO0
HttpFirmwareUpdate airUpdater;

HttpServer server;
int totalActiveSockets = 0;
long nextPos[4];
long curPos[4];
uint8_t step[4];
uint8_t dir[4];
uint32_t deltat = 1000;
String lastPositionMessage = "";


void IRAM_ATTR interruptHandler()
{
	detachInterrupt(UPDATE_PIN);
	procTimer.stop();
	Serial.println("Let's do cloud magic!");

	// Start cloud update
	airUpdater.start();
}

void onIndex(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	//vars["counter"] = String(counter);
	response.sendTemplate(tmpl); // this template object will be deleted automatically
}

void onFile(HttpRequest &request, HttpResponse &response)
{
	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void reportPosition()
{
	// report position only when steppers reached final stage
	// so movement of steppers is not interrupted ba wifi communication
	if (nextPos[0] == curPos[0] && nextPos[1] == curPos[1]
			&& nextPos[2] == curPos[2] && nextPos[3] == curPos[3])
	{
		char buf[30];
		sprintf(buf, "X%d Y%d Z%d E%d", curPos[0], curPos[1], curPos[2],
				curPos[3]);
		String message = String(buf);
		if (!message.equals(lastPositionMessage))
		{
			WebSocketsList &clients = server.getActiveWebSockets();
			for (int i = 0; i < clients.count(); i++)
			{
				clients[i].sendString(message);
			}
			lastPositionMessage = message;
		}
	}
}

void wsConnected(WebSocket& socket)
{
	totalActiveSockets++;
	reportPosition();
	// Notify everybody about new connection
	WebSocketsList &clients = server.getActiveWebSockets();
	for (int i = 0; i < clients.count(); i++)
	{
		clients[i].sendString(
				"New friend arrived! Total: " + String(totalActiveSockets));
	}
}

void blink1()
{
	system_soft_wdt_feed();
	for (int i = 0; i < 4; i++)
	{
		if (curPos[i] != nextPos[i])
		{
			int8_t sign = -1;
			if (nextPos[i] > curPos[i])
				sign = 1;
			if (sign > 0)
				digitalWrite(dir[i], false);
			else
				digitalWrite(dir[i], true);
			delayMicroseconds(3);

			digitalWrite(step[i], false);
			delayMicroseconds(5);
			digitalWrite(step[i], true);
			curPos[i] = curPos[i] + sign;
		}
	}
	reportPosition();
	system_soft_wdt_feed();

}

void wsMessageReceived(WebSocket& socket, const String& message)
{
	Serial.printf("WebSocket message received: %s\r\n", message.c_str());
	//char buf[2];
	//sprintf(buf, "OK");
	//socket.sendString(buf);

	String commandLine;
	commandLine = message.c_str();

	Vector<String> commandToken;
	int numToken = splitString(commandLine, ' ', commandToken);
	for (int i = 0; i < numToken; i++)
	{
		Serial.printf("Command: %s\r\n", commandToken[i].c_str());
		String motor = commandToken[i].substring(0, 1);
		String sign = commandToken[i].substring(1, 2);
		String posStr = "";
		if (sign == "+" || sign == "-")
		{
			posStr = commandToken[i].substring(2, commandToken[i].length());
		}
		else
		{
			sign = "";
			posStr = commandToken[i].substring(1, commandToken[i].length());
		}
		int8_t index = -1;
		if (motor == "X")
			index = 0;
		else if (motor == "Y")
			index = 1;
		else if (motor == "Z")
			index = 2;
		else if (motor == "E")
			index = 3;
		else if (motor == "T")
		{
			deltat = atoi(posStr.c_str());
			procTimer.initializeUs(deltat, blink1).restart();
		}
		if (index > -1)
		{
			if (sign == "+")
				nextPos[index] = nextPos[index] + atol(posStr.c_str());
			else if (sign == "-")
				nextPos[index] = nextPos[index] - atol(posStr.c_str());
			else
				nextPos[index] = atol(posStr.c_str());
			Serial.printf("Set nextpos[%d] to %d\r\n", index, nextPos[index]);
		}
	}
}

void wsBinaryReceived(WebSocket& socket, uint8_t* data, size_t size)
{
	Serial.printf("Websocket binary data receieved, size: %d\r\n", size);
}

void wsDisconnected(WebSocket& socket)
{
	totalActiveSockets--;
}



void startWebServer()
{
	system_soft_wdt_feed();
	Serial.println("Starting web server...Phase1");

	detachInterrupt(UPDATE_PIN);

	//---------------------
	step[0] = 2;  //2
	dir[0] = 0;   //0

	step[1] = 4;  //4
	dir[1] = 5;   //5
	//---------------------
	step[2] = 13;
	dir[2] = 12;

	step[3] = 14;
	dir[3] = 16;
	//---------------------
	system_soft_wdt_feed();

	for (int i = 0; i < 4; i++)
	{
		pinMode(step[i], OUTPUT);
		pinMode(dir[i], OUTPUT);
		digitalWrite(step[i], true);
		digitalWrite(dir[i], true);
		curPos[i] = 0;
		nextPos[i] = 0;
	}
	system_soft_wdt_feed();

	Serial.println("Starting web server...Phase2");
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
	system_soft_wdt_feed();

	procTimer.initializeUs(deltat, blink1).start(true);
}

// Will be called when WiFi station was connected to AP
void connectOk()
{
	system_soft_wdt_feed();
	Serial.println("I'm CONNECTED");
	Serial.println("IP: ");
	Serial.println(WifiStation.getIP().toString());

	airUpdater.addItem(0x0000, "http://192.168.1.23/0x00000.bin");
	airUpdater.addItem(0x9000, "http://192.168.1.23/0x09000.bin");

	Serial.println("You have 5 sec to press program button for air update.");
	attachInterrupt(UPDATE_PIN, interruptHandler, CHANGE);

	system_soft_wdt_feed();
	procTimer.initializeMs(5000, startWebServer).startOnce();
}

void couldntConnect()
{
	Serial.println("Couldn't connect");
}


void init()
{
	Serial.println("Init running...");

	Serial.systemDebugOutput(true);
	System.setCpuFrequency(eCF_160MHz);
	system_soft_wdt_stop();

	WifiAccessPoint.enable(false);
	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiStation.enable(true);

	WifiStation.waitConnection(connectOk);
	Serial.println("Init ended.");
}
