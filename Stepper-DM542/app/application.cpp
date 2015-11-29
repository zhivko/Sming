#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Wiring/SplitString.h>
#include <rboot/rboot.h>
#include <rboot/appcode/rboot-api.h>
#include <HardwareTimer.h>

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put you SSID and Password here
#define WIFI_PWD "PleaseEnterPass"
#endif

Timer reportTimer;

#define UPDATE_PIN 0 // GPIO0
HttpFirmwareUpdate airUpdater;

HttpServer server;
int totalActiveSockets = 0;
long nextPos[4];
long curPos[4];
uint8_t step[4];
uint8_t dir[4];
uint32_t deltat = 2000;
String lastPositionMessage = "";

rBootHttpUpdate* otaUpdater = 0;

String ROM_0_URL = "http://192.168.1.24/rom0.bin";
String SPIFFS_URL = "http://192.168.1.24/spiff_rom.bin";

void reportPosition()
{
	// report position only when steppers reached final stage
	// so movement of steppers is not interrupted ba wifi communication
	//if (nextPos[0] == curPos[0] && nextPos[1] == curPos[1]
	//		&& nextPos[2] == curPos[2] && nextPos[3] == curPos[3])
	//{
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
	//}
}

void IRAM_ATTR StepperTimerInt()
{
	hardwareTimer.initializeUs(deltat, StepperTimerInt);
	hardwareTimer.startOnce();
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
	//reportPosition();
}

void OtaUpdate_CallBack(bool result)
{
	if (result == true)
	{
		// success
		uint8 slot;
		slot = rboot_get_current_rom();
		if (slot == 0)
			slot = 1;
		else
			slot = 0;
		// set to boot new rom and then reboot
		Serial.printf("Firmware updated, rebooting to rom %d...\r\n", slot);
		rboot_set_current_rom(slot);
		System.restart();
	}
	else
	{
		// fail
		Serial.println("Firmware update failed!");
	}
}

void ShowInfo()
{
	Serial.printf("\r\nSDK: v%s\r\n", system_get_sdk_version());
	Serial.printf("Free Heap: %d\r\n", system_get_free_heap_size());
	Serial.printf("CPU Frequency: %d MHz\r\n", system_get_cpu_freq());
	Serial.printf("System Chip ID: %x\r\n", system_get_chip_id());
	Serial.printf("SPI Flash ID: %x\r\n", spi_flash_get_id());
	//Serial.printf("SPI Flash Size: %d\r\n", (1 << ((spi_flash_get_id() >> 16) & 0xff)));

	Vector<String> files = fileList();
	if (files.count() > 0)
	{
		Serial.println("\n\rSpiff files:");
		Serial.println("----------------------------");
		{
			for (int i = 0; i < files.count(); i++)
			{
				Serial.println(files[i]);
			}
		}
		Serial.println("----------------------------");
	}
	else
	{
		Serial.println("Empty spiffs!");
	}

}

void Switch()
{
	uint8 before, after;
	before = rboot_get_current_rom();
	if (before == 0)
		after = 1;
	else
		after = 0;
	Serial.printf("Swapping from rom %d to rom %d.\r\n", before, after);
	rboot_set_current_rom(after);
	Serial.println("Restarting...\r\n");
	System.restart();
}

void OtaUpdate()
{

	hardwareTimer.stop();

	uint8 slot;
	rboot_config bootconf;

	// need a clean object, otherwise if run before and failed will not run again
	if (otaUpdater)
		delete otaUpdater;
	otaUpdater = new rBootHttpUpdate();

	// select rom slot to flash
	bootconf = rboot_get_config();
	slot = bootconf.current_rom;
	if (slot == 0)
		slot = 1;
	else
		slot = 0;

#ifndef RBOOT_TWO_ROMS
	// flash rom to position indicated in the rBoot config rom table
	otaUpdater->addItem(bootconf.roms[slot], ROM_0_URL);
#else
	// flash appropriate rom
	if (slot == 0)
	{
		otaUpdater->addItem(bootconf.roms[slot], ROM_0_URL);
	}
	else
	{
		otaUpdater->addItem(bootconf.roms[slot], ROM_1_URL);
	}
#endif

	// use user supplied values (defaults for 4mb flash in makefile)
	if (slot == 0)
	{
		otaUpdater->addItem(RBOOT_SPIFFS_0, SPIFFS_URL);
	}
	else
	{
		otaUpdater->addItem(RBOOT_SPIFFS_1, SPIFFS_URL);
	}

	// set a callback
	otaUpdater->setCallback(OtaUpdate_CallBack);

	// start update
	otaUpdater->start();
}

void serialCallBack(Stream& stream, char arrivedChar,
		unsigned short availableCharsCount)
{
	int ia = (int) arrivedChar;
	if (ia == 13)
	{
		char str[availableCharsCount];
		for (int i = 0; i < availableCharsCount; i++)
		{
			str[i] = stream.read();
			if (str[i] == '\r' || str[i] == '\n')
			{
				str[i] = '\0';
			}
		}

		if (!strcmp(str, "connect"))
		{
			// connect to wifi
			WifiStation.config(WIFI_SSID, WIFI_PWD);
			WifiStation.enable(true);
		}
		else if (!strcmp(str, "ip"))
		{
			Serial.printf("ip: %s mac: %s\r\n",
					WifiStation.getIP().toString().c_str(),
					WifiStation.getMAC().c_str());
		}
		else if (!strcmp(str, "ota"))
		{
			OtaUpdate();
		}
		else if (!strcmp(str, "restart"))
		{
			System.restart();
		}
		else if (!strcmp(str, "ls"))
		{
			Vector<String> files = fileList();
			Serial.printf("filecount %d\r\n", files.count());
			for (unsigned int i = 0; i < files.count(); i++)
			{
				Serial.println(files[i]);
			}
		}
		else if (!strcmp(str, "info"))
		{
			ShowInfo();
		}
		else if (!strcmp(str, "pos"))
		{
			reportPosition();
		}
		else if (!strcmp(str, "move"))
		{
			Serial.println();
			nextPos[0] += 10000;
			nextPos[1] += 10000;
			nextPos[2] += 10000;
			nextPos[3] += 10000;
			//procTimer.initializeUs(deltat, blink1).start(true);
		}
		else if (!strcmp(str, "help"))
		{
			Serial.println();
			Serial.println("available commands:");
			Serial.println("  help - display this message");
			Serial.println("  ip - show current ip address");
			Serial.println("  connect - connect to wifi");
			Serial.println("  restart - restart the esp8266");
			Serial.println("  switch - switch to the other rom and reboot");
			Serial.println("  ota - perform ota update, switch rom and reboot");
			Serial.println("  info - show esp8266 info");
#ifndef DISABLE_SPIFFS
			Serial.println("  ls - list files in spiffs");
			Serial.println("  cat - show first file in spiffs");
#endif
			Serial.println();
		}
		else
		{
			Serial.println("unknown command");
		}
	}
}

void IRAM_ATTR interruptHandler()
{
	detachInterrupt(UPDATE_PIN);
	Serial.println("Let's do cloud magic!");

	// Start cloud update
	OtaUpdate();
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

void wsConnected(WebSocket& socket)
{
	totalActiveSockets++;
	lastPositionMessage = "";
	// Notify everybody about new connection
	/*
	 WebSocketsList &clients = server.getActiveWebSockets();
	 for (int i = 0; i < clients.count(); i++)
	 {
	 clients[i].sendString(
	 "New friend arrived! Total: " + String(totalActiveSockets));
	 }
	 */
}

void wsMessageReceived(WebSocket& socket, const String& message)
{
	Serial.printf("WebSocket message received: %s\r\n", message.c_str());
	//char buf[2];
	//sprintf(buf, "OK");
	//socket.sendString(buf);
	String commandLine;
	commandLine = message.c_str();

	if (commandLine.equals("flash"))
	{
		server.enableWebSockets(false);
		OtaUpdate();
		return;
	}
	else if (commandLine.equals("pos"))
	{
		reportPosition();
		return;
	}

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

void initPins()
{
	Serial.println("Init pins");
	//---------------------
	step[0] = 2;  //2
	dir[0] = 0;   //0

	step[1] = 4;  //4
	dir[1] = 5;   //5
	//---------------------
	step[2] = 13;
	dir[2] = 12;

	step[3] = 16;
	dir[3] = 14;
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
}

void startWebServer()
{
	system_soft_wdt_feed();
	detachInterrupt(UPDATE_PIN);
	Serial.println("Starting web server...Phase1");
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
}

// Will be called when WiFi station was connected to AP
void connectOk()
{
	system_soft_wdt_feed();
	Serial.println("I'm CONNECTED");
	Serial.println("IP: ");
	Serial.println(WifiStation.getIP().toString());

	startWebServer();

	Serial.println("Init ended.");
	Serial.println("Type 'help' and press enter for instructions.");
	Serial.println();
	Serial.setCallback(serialCallBack);

	reportTimer.initializeMs(1000, reportPosition).start();

	hardwareTimer.initializeUs(deltat, StepperTimerInt);
	hardwareTimer.startOnce();

}

void couldntConnect()
{
	Serial.println("Couldn't connect");
}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.println("Init running...");

	Serial.systemDebugOutput(true);
	System.setCpuFrequency(eCF_160MHz);
	//system_soft_wdt_stop();

	int slot = rboot_get_current_rom();
#ifndef DISABLE_SPIFFS
	if (slot == 0)
	{
#ifdef RBOOT_SPIFFS_0
		debugf("trying to mount spiffs at %x, length %d", RBOOT_SPIFFS_0 + 0x40200000, SPIFF_SIZE);
		spiffs_mount_manual(RBOOT_SPIFFS_0 + 0x40200000, SPIFF_SIZE);
#else
		debugf("trying to mount spiffs at %x, length %d", 0x40300000,
				SPIFF_SIZE);
		spiffs_mount_manual(0x40300000, SPIFF_SIZE);
#endif
	}
	else
	{
#ifdef RBOOT_SPIFFS_1
		debugf("trying to mount spiffs at %x, length %d", RBOOT_SPIFFS_1 + 0x40200000, SPIFF_SIZE);
		spiffs_mount_manual(RBOOT_SPIFFS_1 + 0x40200000, SPIFF_SIZE);
#else
		debugf("trying to mount spiffs at %x, length %d", 0x40500000,
				SPIFF_SIZE);
		spiffs_mount_manual(0x40500000, SPIFF_SIZE);
#endif
	}
#else
	debugf("spiffs disabled");
#endif
	ShowInfo();

	initPins();

	WifiAccessPoint.enable(false);
	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiStation.enable(true);

	WifiStation.waitConnection(connectOk);

}
