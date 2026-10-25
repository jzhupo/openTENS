#if defined(OPENTENS_USE_ESP32_IDF)

#include <cstdio>
#include <cstring>
#include <string>
#include <dirent.h>
#include <sys/stat.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_littlefs.h"
#include "esp_http_server.h"

#include "channel.hpp"


static const char *FS_BASE_PATH = "/spiffs";

// Web服务器句柄
static httpd_handle_t server = nullptr;

// WiFi连接同步事件组
static EventGroupHandle_t s_wifi_event_group = nullptr;
static constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
static bool s_sta_connecting = false;


// 内部辅助：安全的十六进制字符转数值（避免 GCC 15 sscanf 格式警告）
static inline int hexCharToInt(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

// 内部辅助：URL参数解码（适配 GCC 15 -Werror 类型检查）
static std::string decodeURL(const char *str) {
	if (str == nullptr) {
		return "";
	}
	std::string ret;
	const size_t len = strlen(str);
	ret.reserve(len);

	for (size_t i = 0; i < len; ++i) {
		if (str[i] == '%' && (i + 2) < len) {
			int high = hexCharToInt(str[i + 1]);
			int low  = hexCharToInt(str[i + 2]);
			if (high == 0 && low == 0) {
				break;
			} else if (high >= 0 && low >= 0) {
				ret += static_cast<char>((high << 4) | low);
				i += 2;
				continue;
			}
		} else if (str[i] == '+') {
			ret += ' ';
			continue;
		}
		ret += str[i];
	}
	return ret;
}

// 内部辅助：读取单行配置并去除首尾空白符 (兼容 Picolibc stdio)
static std::string readFirstLineTrimmed(const char *filepath) {
	FILE *f = fopen(filepath, "r");
	if (f == nullptr) {
		return "";
	}
	char buf[128] = {0};
	if (fgets(buf, sizeof(buf), f) == nullptr) {
		fclose(f);
		return "";
	}
	fclose(f);
	std::string s(buf);
	const char *ws = " \t\n\r\f\v";
	const size_t last = s.find_last_not_of(ws);
	if (last == std::string::npos) {
		return "";
	}
	s.erase(last + 1);
	s.erase(0, s.find_first_not_of(ws));
	return s;
}

// 内部辅助：以分块方式发送 LittleFS 中的文件
static esp_err_t sendFileFromLittleFS(httpd_req_t *req, std::string name, const char *content_type) {
	// 自动兼容是否携带前导 '/' 并拼接 FS_BASE_PATH
	std::string filename = std::string(FS_BASE_PATH);
	if (name.empty() || name[0] != '/') {
		filename += '/';
	}
	filename += name;
	FILE *f = fopen(filename.c_str(), "r");
	if (f == nullptr) {
		if (httpd_resp_set_status(req, "404 Not Found") != ESP_OK) {
			return ESP_FAIL;
		}
		if (httpd_resp_set_type(req, "text/plain") != ESP_OK) {
			return ESP_FAIL;
		}
		return httpd_resp_sendstr(req, "File not found");
	}
	if (httpd_resp_set_type(req, content_type) != ESP_OK) {
		fclose(f);
		return ESP_FAIL;
	}
	char chunk[512]; // 配合 Picolibc 保持较小的栈占用
	size_t read_bytes = 0;
	while ((read_bytes = fread(chunk, 1, sizeof(chunk), f)) > 0) {
		if (httpd_resp_send_chunk(req, chunk, static_cast<ssize_t>(read_bytes)) != ESP_OK) {
			fclose(f);
			httpd_resp_sendstr_chunk(req, nullptr);
			return ESP_FAIL;
		}
	}
	fclose(f);
	return httpd_resp_send_chunk(req, nullptr, 0);
}


// WiFi与IP事件回调
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	static_cast<void>(arg);
	static_cast<void>(event_data);
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_sta_connecting) {
			esp_wifi_connect();
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

// ================= HTTP 路由处理函数 =================

static esp_err_t getHandlerOnIndex(httpd_req_t *req) {
	return sendFileFromLittleFS(req, std::string("/index.html"), "text/html; charset=utf-8");
}

static esp_err_t getHandlerOnStatus(httpd_req_t *req) {
	std::string json;
	json.reserve(1024 + channelA.raw_waveform.length() + channelB.raw_waveform.length());
	json = "{\"channelA\":{\"running\":";
	json += channelA.running ? "true" : "false";
	json += ",\"intensity\":";
	json += std::to_string(channelA.global_intensity);
	json += ",\"waveform\":\"";
	json += channelA.raw_waveform;
	json += "\",\"mirror\":";
	json += (channelA.mirror != nullptr) ? "true" : "false";
	json += "},\"channelB\":{\"running\":";
	json += channelB.running ? "true" : "false";
	json += ",\"intensity\":";
	json += std::to_string(channelB.global_intensity);
	json += ",\"waveform\":\"";
	json += channelB.raw_waveform;
	json += "\"}}";

	httpd_resp_set_type(req, "application/json");
	return httpd_resp_send(req, json.c_str(), static_cast<ssize_t>(json.length()));
}

static esp_err_t postHandlerOnControl(httpd_req_t *req) {
	// 提前统一设置默认响应类型为文本
	httpd_resp_set_type(req, "text/plain");

	const size_t buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len <= 1) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Missing channel parameter");
	}

	std::string query_buf(buf_len, '\0');
	if (httpd_req_get_url_query_str(req, query_buf.data(), buf_len) != ESP_OK) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Missing channel parameter");
	}

	char param_channel[16] = {0};
	if (httpd_query_key_value(query_buf.c_str(), "channel", param_channel, sizeof(param_channel)) != ESP_OK) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Missing channel parameter");
	}

	char param_action[32] = {0};
	if (httpd_query_key_value(query_buf.c_str(), "action", param_action, sizeof(param_action)) != ESP_OK) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Missing action parameter");
	}

	Channel_t *channel = getChannelByKey(param_channel);
	if (channel == nullptr) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Invalid channel parameter");
	}

	const std::string action(param_action);
	if (action == "start") {
		const size_t waveform_length = req->content_len;
		if (waveform_length == 0) {
			httpd_resp_set_status(req, "400 Bad Request");
			return httpd_resp_sendstr(req, "Missing waveform body");
		}
		if (waveform_length > WAVEFORM_MAX_STRING_LENGTH) {
			httpd_resp_set_status(req, "413 Payload Too Large");
			return httpd_resp_sendstr(req, "Oversized waveform body");
		}
		std::string waveform_string(waveform_length, '\0');
		size_t received = 0;
		while (received < waveform_length) {
			const int ret = httpd_req_recv(req, &waveform_string[received], waveform_length - received);
			if (ret <= 0) {
				if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
					httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Waveform body receive timeout");
					return ESP_FAIL;
				}
				httpd_resp_set_status(req, "500 Internal Server Error");
				return httpd_resp_sendstr(req, "Failed to receive waveform body");
			}
			received += static_cast<size_t>(ret);
		}
		if (!handleStart(channel, waveform_string.c_str())) {
			httpd_resp_set_status(req, "400 Bad Request");
			return httpd_resp_sendstr(req, "Failed to start channel");
		}
		updatePinOutput(LED_PIN, false);
	} else if (action == "stop") {
		handleStop(channel);
		if (!channelA.running && !channelB.running) {
			updatePinOutput(LED_PIN, true);
		}
	} else if (action == "plus") {
		handleIntensity(channel, true);
	} else if (action == "minus") {
		handleIntensity(channel, false);
	} else if (action == "mirror") {
		char param_enable[8] = {0};
		if (httpd_query_key_value(query_buf.c_str(), "enable", param_enable, sizeof(param_enable)) != ESP_OK) {
			httpd_resp_set_status(req, "400 Bad Request");
			return httpd_resp_sendstr(req, "Missing enable parameter");
		}
		const bool enable = (strcmp(param_enable, "1") == 0);
		handleMirror(enable);
		if (!channelA.running && !channelB.running) {
			updatePinOutput(LED_PIN, true);
		}
	} else {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Invalid action parameter");
	}

	return httpd_resp_sendstr(req, "OK");
}

static esp_err_t getHandlerOnList(httpd_req_t *req) {
	std::string json = "[";
	DIR *dir = opendir(FS_BASE_PATH);
	if (dir != nullptr) {
		struct dirent *entry = nullptr;
		bool first = true;
		const std::string suffix = ".pulse";
		while ((entry = readdir(dir)) != nullptr) {
			if (entry->d_type == DT_DIR) {
				continue;
			}
			std::string name(entry->d_name);
			if (name.length() >= suffix.length() &&
				name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0) {
				std::string filename = std::string(FS_BASE_PATH) + "/" + name;
				struct stat st = {};
				if (stat(filename.c_str(), &st) == 0) {
					if (!first) {
						json += ",";
					}
					json += "{\"name\":\"" + name + "\",\"size\":" + std::to_string(st.st_size) + "}";
					first = false;
				}
			}
		}
		closedir(dir);
	}
	json += "]";

	httpd_resp_set_type(req, "application/json");
	return httpd_resp_send(req, json.c_str(), static_cast<ssize_t>(json.length()));
}

static esp_err_t getHandlerOnPulse(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/plain");
	const size_t buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len <= 1) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Missing file parameter");
	}

	std::string query_buf(buf_len, '\0');
	char param_name[128] = {0};
	if (httpd_req_get_url_query_str(req, query_buf.data(), buf_len) != ESP_OK ||
		httpd_query_key_value(query_buf.c_str(), "name", param_name, sizeof(param_name)) != ESP_OK) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Missing file parameter");
	}

	std::string name = decodeURL(param_name);
	if ((name.size() <= 6) || (name.compare(name.size() - 6, 6, ".pulse") != 0)) {
		httpd_resp_set_status(req, "400 Bad Request");
		return httpd_resp_sendstr(req, "Invalid pulse name");
	}

	return sendFileFromLittleFS(req, name, "text/plain");
}

// ================= 主初始化与循环 =================

void setupESP32() {
	// 1. 初始化 NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// 3. 挂载 LittleFS
	esp_vfs_littlefs_conf_t conf = {};
	conf.base_path              = FS_BASE_PATH;
	conf.partition_label        = "spiffs"; // 对应 partitions.csv 中的分区名
	conf.format_if_mount_failed = true;
	conf.dont_mount             = false;
	ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));

	// 4. 读取 WiFi 凭据
	const std::string sta_ssid     = readFirstLineTrimmed("/spiffs/wifi_ssid.txt");
	const std::string sta_password = readFirstLineTrimmed("/spiffs/wifi_password.txt");

	// 5. 初始化网络接口与 WiFi
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	s_wifi_event_group = xEventGroupCreate();
	esp_event_handler_instance_t instance_any_id = nullptr;
	esp_event_handler_instance_t instance_got_ip = nullptr;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
		&wifi_event_handler, nullptr, &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
		&wifi_event_handler, nullptr, &instance_got_ip));

	bool wifi_connected = false;
	if (!sta_ssid.empty()) {
		wifi_config_t wifi_config = {};
		strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid),
			sta_ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
		strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
			sta_password.c_str(), sizeof(wifi_config.sta.password) - 1);

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
		ESP_ERROR_CHECK(esp_wifi_start());

		s_sta_connecting = true;
		esp_wifi_connect();

		// 阻塞等待最多 30 秒
		const EventBits_t bits = xEventGroupWaitBits(
			s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
		if ((bits & WIFI_CONNECTED_BIT) != 0) {
			wifi_connected = true;
		}
	}

	// 6. STA 连接超时则切换至 SoftAP 模式
	if (!wifi_connected) {
		s_sta_connecting = false;
		if (!sta_ssid.empty()) {
			esp_wifi_disconnect();
			esp_wifi_stop();
		}

		wifi_config_t ap_config = {};
		strcpy(reinterpret_cast<char*>(ap_config.ap.ssid), "openTENS");
		ap_config.ap.ssid_len       = static_cast<uint8_t>(strlen("openTENS"));
		strcpy(reinterpret_cast<char*>(ap_config.ap.password), "openTENS");
		ap_config.ap.max_connection = 4;
		ap_config.ap.authmode       = WIFI_AUTH_WPA2_PSK;

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
		ESP_ERROR_CHECK(esp_wifi_start());

		wifi_connected = true;
	}
#if defined(ARDUINO_ESP32C3_DEV) || defined(ARDUINO_LOLIN_C3_MINI)
	if (wifi_connected) {
		// 限制发射功率 8.5dBm (34 * 0.25dBm = 8.5dBm，必须在 esp_wifi_start 之后调用)
		ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(34));
	}
#endif

	// 7. 启动 HTTP Server 并注册路由
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.recv_wait_timeout = 1;
	config.max_uri_handlers = 8;
	config.stack_size       = 8192;

	if (httpd_start(&server, &config) == ESP_OK) {
		const httpd_uri_t uri_index = {
			.uri = "/",
			.method = HTTP_GET,
			.handler = getHandlerOnIndex,
			.user_ctx = nullptr,
		};
		const httpd_uri_t uri_status = {
			.uri = "/status",
			.method = HTTP_GET,
			.handler = getHandlerOnStatus,
			.user_ctx = nullptr,
		};
		const httpd_uri_t uri_control = {
			.uri = "/control",
			.method = HTTP_POST,
			.handler = postHandlerOnControl,
			.user_ctx = nullptr,
		};
		const httpd_uri_t uri_list = {
			.uri = "/list",
			.method = HTTP_GET,
			.handler = getHandlerOnList,
			.user_ctx = nullptr,
		};
		const httpd_uri_t uri_pulse = {
			.uri = "/pulse",
			.method = HTTP_GET,
			.handler = getHandlerOnPulse,
			.user_ctx = nullptr,
		};
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_index));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_status));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_control));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_list));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_pulse));
	}
}

#endif//OPENTENS_USE_ESP32_IDF
