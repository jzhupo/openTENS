#if defined(OPENTENS_USE_ESP32_ARDUINO)

#include <WiFi.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "channel.hpp"


// Web接口
static AsyncWebServer server(80);


void setupESP32() {
	// LittleFS
	LittleFS.begin(true);

	// STA模式WiFi
	String sta_ssid;
	String sta_password;
	if (LittleFS.exists("/wifi_ssid.txt")) {
		File ssid_file = LittleFS.open("/wifi_ssid.txt", "r");
		if (ssid_file) {
			String temp = ssid_file.readStringUntil('\n');
			temp.trim();
			if (temp.length() > 0) {
				sta_ssid = temp;
			}
			ssid_file.close();
		}
	}
	if (LittleFS.exists("/wifi_password.txt")) {
		File password_file = LittleFS.open("/wifi_password.txt", "r");
		if (password_file) {
			String temp = password_file.readStringUntil('\n');
			temp.trim();
			if (temp.length() > 0) {
				sta_password = temp;
			}
			password_file.close();
		}
	}
	bool wifi_connected = false;
	if (sta_ssid.length() > 0) {
		WiFi.mode(WIFI_STA);
		WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
		const uint32_t sta_start_ms = millis();
		while (millis() - sta_start_ms < 30000) {
			if (WiFi.status() == WL_CONNECTED) {
				wifi_connected = true;
				break;
			}
			delay(500);
		}
	}
	// AP模式连接WiFi
	if (!wifi_connected) {
		if (sta_ssid.length() > 0) {
			WiFi.disconnect();
		}
		WiFi.mode(WIFI_AP);
		WiFi.softAP("openTENS", "openTENS");
		wifi_connected = true;
	}
#if defined(ARDUINO_ESP32C3_DEV) || defined(ARDUINO_LOLIN_C3_MINI)
	if (wifi_connected) {
		// 限制发射功率 8.5dBm
		WiFi.setTxPower(WIFI_POWER_8_5dBm);
	}
#endif

	// 注册Web服务器路由
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		const char *filename = "/index.html";
		if (LittleFS.exists(filename)) {
			request->send(LittleFS, filename, "text/html");
		} else {
			request->send(404, "text/plain", "Error: index.html not found in LittleFS!");
		}
	});

	server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
		String json;
		json.reserve(1024 + channelA.raw_waveform.length() + channelB.raw_waveform.length());
		json = "{\"channelA\":{\"running\":";
		json += channelA.running ? "true" : "false";
		json += ",\"intensity\":";
		json += channelA.global_intensity;
		json += ",\"waveform\":\"";
		json += channelA.raw_waveform;
		json += "\",\"mirror\":";
		json += (channelA.mirror != nullptr) ? "true" : "false";
		json += "},\"channelB\":{\"running\":";
		json += channelB.running ? "true" : "false";
		json += ",\"intensity\":";
		json += channelB.global_intensity;
		json += ",\"waveform\":\"";
		json += channelB.raw_waveform;
		json += "\"}}";
		request->send(200, "application/json", json);
	});

	server.on("/control", HTTP_POST,
		[](AsyncWebServerRequest *request){
			// 除了读取 body 以外的控制逻辑
			const AsyncWebParameter* param_channel = request->getParam("channel");
			if (param_channel == nullptr) {
				request->send(400, "text/plain", "Missing channel parameter");
				return;
			}
			const AsyncWebParameter* param_action = request->getParam("action");
			if (param_action == nullptr) {
				request->send(400, "text/plain", "Missing action parameter");
				return;
			}
			Channel_t *channel = getChannelByKey(param_channel->value().c_str());
			if (channel == nullptr) {
				request->send(400, "text/plain", "Invalid channel parameter");
				return;
			}
			const String action = param_action->value();
			if (action == "start") {
				// 波形启动
				if (request->contentLength() > WAVEFORM_MAX_STRING_LENGTH) {
					request->send(413, "text/plain", "Oversized waveform body");
					return;
				}
				if (request->_tempObject == nullptr) {
					request->send(400, "text/plain", "Missing waveform body");
					return;
				}
				const char *waveform_string = static_cast<char *>(request->_tempObject);
				if (!handleStart(channel, waveform_string)) {
					request->send(400, "text/plain", "Failed to start channel");
					return;
				}
				// 任一通道启动时点亮LED
				updatePinOutput(LED_PIN, false);
			} else if (action == "stop") {
				// 波形停止
				handleStop(channel);
				// 所有通道关闭时熄灭LED
				if (!channelA.running && !channelB.running)
				{
					updatePinOutput(LED_PIN, true);
				}
			} else if (action == "plus") {
				// 强度增加
				handleIntensity(channel, true);
			} else if (action == "minus") {
				// 强度减少
				handleIntensity(channel, false);
			} else if (action == "mirror") {
				const AsyncWebParameter* param_enable = request->getParam("enable");
				if (param_enable == nullptr) {
					request->send(400, "text/plain", "Missing enable parameter");
					return;
				}
				const bool enable = (param_enable->value() == "1");
				handleMirror(enable);
				// 所有通道关闭时熄灭LED
				if (!channelA.running && !channelB.running)
				{
					updatePinOutput(LED_PIN, true);
				}
			} else {
				request->send(400, "text/plain", "Invalid action parameter");
				return;
			}
			request->send(200, "text/plain", "OK");
		},
		nullptr, // FileUpload 处理器，这里不需要，传 nullptr
		[](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
			// body 数据块处理器：用于异步、分段接收超长 POST 数据
			// 首次接收数据时，在 request 上附加一个空的 body 参数
			if (index == 0) {
				if (total > WAVEFORM_MAX_STRING_LENGTH) {
					return;
				}
				request->_tempObject = malloc(total + 1);
				if (request->_tempObject != nullptr) {
					static_cast<char *>(request->_tempObject)[total] = '\0';
				}
			}
			if ((request->_tempObject != nullptr) && (len > 0)) {
				memcpy(static_cast<char *>(request->_tempObject) + index, data, len);
			}
		}
	);

	server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
		String json = "[";
		File root = LittleFS.open("/");
		if (root && root.isDirectory()) {
			File file = root.openNextFile();
			bool first = true;
			while (file) {
				String name = file.name();
				if (name.startsWith("/")) {
					name = name.substring(1);
				}
				if (name.endsWith(".pulse")) {
					if (!first) {
						json += ",";
					}
					json += "{\"name\":\"" + name + "\",\"size\":" + String(file.size()) + "}";
					first = false;
				}
				file = root.openNextFile();
			}
			root.close();
		}
		json += "]";
		request->send(200, "application/json", json);
	});

	server.on("/pulse", HTTP_GET, [](AsyncWebServerRequest *request){
		const AsyncWebParameter* pName = request->getParam("name");
		if (pName == nullptr) {
			request->send(400, "text/plain", "Missing file parameter");
			return;
		}
		String path = pName->value();
		if ((path.length() != strlen(path.c_str())) || (path.length() <= 6) || !path.endsWith(".pulse")) {
			request->send(400, "text/plain", "Invalid pulse name");
			return;
		}
		if (!path.startsWith("/")) {
			path = "/" + path;
		}

		if (LittleFS.exists(path)) {
			request->send(LittleFS, path, "text/plain");
		} else {
			request->send(404, "text/plain", "File not found");
		}
	});

	server.begin();
}

void loopESP32() {
}

#endif//OPENTENS_USE_ESP32_ARDUINO
