#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(__TINYC__)
#include <stdio.h>
#define DG_LAB_ERROR(...) printf(__VA_ARGS__)
#else
#define DG_LAB_ERROR(...) ((void)0)
#endif
#include "channel.hpp"

// 协议映射查找表
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
DRAM_ATTR
#endif
const uint16_t FREQ_SLIDER_VALUE_MAP[] = {
	//  0~40:  10~  50 步进 1
	// 41~55:  52~  80 步进 2
	// 56~59:  85~ 100 步进 5
	// 60~69: 110~ 200 步进 10
	// 70~75: 233~ 400 步进 33.3
	// 76~79: 450~ 600 步进 50
	// 80~83: 700~1000 步进 100
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
	80, 85, 90, 95,
	100, 110, 120, 130, 140, 150, 160, 170, 180, 190,
	200, 233, 266, 300, 333, 366,
	400, 450, 500, 550,
	600, 700, 800, 900, 1000,
};
constexpr size_t FREQ_MAP_SIZE = sizeof(FREQ_SLIDER_VALUE_MAP) / sizeof(uint16_t);

#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
DRAM_ATTR
#endif
const uint16_t SECTION_TIME_MAP[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
	52, 54, 56, 58, 60, 62, 64, 66, 68, 70,
	72, 74, 76, 78, 80, 85, 90, 95,
	100, 110, 120, 130, 140, 150, 160, 170, 180, 190,
	200, 234, 266, 300, 334, 366,
	400, 450, 500, 550,
	600, 700, 800, 900,
	1000, 1200, 1400, 1600, 1800,
	2000, 2500, 3000,
};
constexpr size_t TIME_MAP_SIZE = sizeof(SECTION_TIME_MAP) / sizeof(uint16_t);


#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF) || defined(__TINYC__)
bool parseWaveform(Waveform_t *target, const char *source) {
	if (target == nullptr || source == nullptr) {
		return false;
	}

	// 初始化
	memset(target, 0, sizeof(Waveform_t));
	target->speed_factor = 1; // 默认播放速度因子

	// 校验协议头
	const char *prefix = "Dungeonlab+pulse:";
	size_t prefix_len = strlen(prefix);
	if (strncmp(source, prefix, prefix_len) != 0) {
		DG_LAB_ERROR("Error: missing \"Dungeonlab+pulse:\" header\n");
		return false;
	}
	const char *p = source + prefix_len;
	char *end;

	// 可选的全局参数
	const char *eq = strchr(p, '=');

	if (eq != nullptr) {
		long rest = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid pause time\n");
			return false;
		}
		if (rest < 0 || rest > 255) {
			DG_LAB_ERROR("Error: pause time (%ld) must >= 0 and <= 255\n", rest);
			return false;
		}
		p = end;
		if (*p == ',') {
			p++;
		} else {
			DG_LAB_ERROR("Error: missing \",\" after pause time\n");
			return false;
		}

		long speed_factor = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid speed factor\n");
			return false;
		}
		if (speed_factor != 1 && speed_factor != 2 && speed_factor != 4) {
			DG_LAB_ERROR("Error: speed factor (%ld) must be 1 or 2 or 4\n", speed_factor);
			return false;
		}
		p = end;
		if (*p == ',') {
			p++;
		} else {
			DG_LAB_ERROR("Error: missing \",\" after speed factor\n");
			return false;
		}

		long frequency_balance = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid frequency balance\n");
			return false;
		}
		if (frequency_balance < 0 || frequency_balance > 255) {
			DG_LAB_ERROR("Error: frequency balance (%ld) must >= 0 and <= 255\n", frequency_balance);
			return false;
		}
		if (end == eq) {
			// 安全步进至首个小节
			p = eq + 1;
		} else {
			// 确保3个参数解析后，精确对齐至'='
			DG_LAB_ERROR("Error: missing \"=\" after frequency balance\n");
			return false;
		}

		if (rest > 0) {
			target->pulse_section_pause_time = (uint8_t)((rest - 1) / 10 + 1);
		}
		target->speed_factor = (uint8_t)speed_factor;
		target->frequency_balance = (uint8_t)frequency_balance;
	}

	uint16_t pool_index = 0;

	// 小节数据
	while (*p != '\0') {
		Section_t new_section;
		memset(&new_section, 0, sizeof(Section_t));

		// 小节头部参数
		long frequency_a = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid frequency A\n");
			return false;
		}
		if (frequency_a < 0 || frequency_a >= FREQ_MAP_SIZE) {
			DG_LAB_ERROR("Error: frequency A (%ld) must >= 0 and < FREQ_MAP_SIZE (%zu)\n", frequency_a, FREQ_MAP_SIZE);
			return false;
		}
		p = end;
		if (*p == ',') {
			p++;
		} else {
			DG_LAB_ERROR("Error: missing \",\" after frequency A\n");
			return false;
		}

		long frequency_b = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid frequency B\n");
			return false;
		}
		if (frequency_b < 0 || frequency_b >= FREQ_MAP_SIZE) {
			DG_LAB_ERROR("Error: frequency B (%ld) must >= 0 and < FREQ_MAP_SIZE (%zu)\n", frequency_b, FREQ_MAP_SIZE);
			return false;
		}
		p = end;
		if (*p == ',') {
			p++;
		} else {
			DG_LAB_ERROR("Error: missing \",\" after frequency B\n");
			return false;
		}

		long pulse_section_time = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid pulse section time\n");
			return false;
		}
		if (pulse_section_time < 0 || pulse_section_time >= TIME_MAP_SIZE) {
			DG_LAB_ERROR("Error: pulse section time (%ld) must >= 0 and < TIME_MAP_SIZE (%zu)\n", pulse_section_time, TIME_MAP_SIZE);
			return false;
		}
		p = end;
		if (*p == ',') {
			p++;
		} else {
			DG_LAB_ERROR("Error: missing \",\" after pulse section time\n");
			return false;
		}

		long frequency_mode = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid frequency mode\n");
			return false;
		}
		if (frequency_mode < 1 || frequency_mode > 4) {
			DG_LAB_ERROR("Error: frequency mode (%ld) must be 1 or 2 or 3 or 4\n", frequency_mode);
			return false;
		}
		p = end;
		if (*p == ',') {
			p++;
		} else {
			DG_LAB_ERROR("Error: missing \",\" after frequency mode\n");
			return false;
		}

		long section_enabled = strtol(p, &end, 10);
		if (end == p) {
			DG_LAB_ERROR("Error: missing or invalid section enabled\n");
			return false;
		}
		if (section_enabled != 0 && section_enabled != 1) {
			DG_LAB_ERROR("Error: section enabled (%ld) must be 0 or 1\n", section_enabled);
			return false;
		}
		p = end;
		if (*p == '/') {
			p++;
		} else {
			DG_LAB_ERROR("Error: missing \"/\" after section enabled\n");
			return false;
		}

		// 脉冲序列
		while (true) {
			long int_part = strtol(p, &end, 10);
			if (end == p) {
				DG_LAB_ERROR("Error: missing or invalid pulse intensity\n");
				return false;
			}
			if (int_part < 0 || int_part > 100) {
				DG_LAB_ERROR("Error: pulse intensity int part (%ld) must >= 0 and <= 100\n", int_part);
				return false;
			}
			p = end;
			long frac_part = 0;

			// 小数位
			if (*p == '.') {
				p++;
				if (*p < '0' || *p > '9') { // 小数点后必须有数字
					DG_LAB_ERROR("Error: missing or invalid digits after \".\" in pulse intensity\n");
					return false;
				}
				int frac_digits = 0;
				while (*p >= '0' && *p <= '9') {
					if (int_part == 100 && *p != '0') { // 禁止100.01
						DG_LAB_ERROR("Error: pulse intensity must <= 100.00\n");
						return false;
					}
					if (frac_digits < 2) {
						// 保留2位有效小数
						frac_part = frac_part * 10 + (*p - '0');
						frac_digits++;
					}
					p++;
				}
				if (frac_digits == 1) {
					// 补齐1位小数情况 (如 .5 -> 50)
					frac_part *= 10;
				}
			}

			if (*p == '-') {
				p++;
			} else {
				DG_LAB_ERROR("Error: missing \"-\" after pulse intensity\n");
				return false;
			}
			// 脉冲类型
			long pulse_anchor = strtol(p, &end, 10);
			if (end == p) {
				DG_LAB_ERROR("Error: missing or invalid pulse anchor\n");
				return false;
			}
			if (pulse_anchor != 0 && pulse_anchor != 1) {
				DG_LAB_ERROR("Error: pulse anchor (%ld) must be 0 or 1\n", pulse_anchor);
				return false;
			}
			p = end;

			if (section_enabled == 0) {
				// 未启用的小节，只累加计数用于校验 (>=2)，不占用 pulses_pool 空间
				new_section.pulse_count++;
			} else if (pool_index + new_section.pulse_count < WAVEFORM_MAX_PULSE_COUNT) {
				Pulse_t *new_pulse = &target->pulses_pool[pool_index + new_section.pulse_count];
				// 定点数运算直接向下取整换算微秒脉宽 (intensity * 1.5):
				// intensity_x100 = 100 * intensity
				// width_us = (intensity_x100 * 1.5) / 100 => (intensity_x100 * 3) / 200
				uint32_t intensity_x100 = (uint32_t)int_part * 100UL + (uint32_t)frac_part;
				new_pulse->width_us = (uint16_t)((intensity_x100 * 3UL) / 200UL);
				new_section.pulse_count++;
			} else {
				DG_LAB_ERROR("Error: pulses pool index (%u + %u) in section (%u) must < WAVEFORM_MAX_PULSE_COUNT (%d)\n",
					pool_index, new_section.pulse_count, target->section_count, WAVEFORM_MAX_PULSE_COUNT);
				return false;
			}

			// 校验脉冲间的分隔符
			if (*p == ',') {
				// 逗号，说明后面还有脉冲元，消费它并继续循环
				p++;
			} else if (*p == '+' || *p == '\0') {
				// 加号（准备解析下一个小节）或结束符，直接跳出脉冲体解析
				break;
			} else {
				DG_LAB_ERROR("Error: missing \",\" or \"+section+\" or \"\\0\" after pulse anchor\n");
				return false;
			}
		}

		// 任何小节必须至少包含2个脉冲点
		if (new_section.pulse_count < 2) {
			DG_LAB_ERROR("Error: pulse count (%u) must >= 2\n", new_section.pulse_count);
			return false;
		}

		// 仅当 section enabled 时写入 struct
		if (section_enabled > 0) {
			// 计算时序
			new_section.frequency_mode = (uint8_t)frequency_mode;
			new_section.unit_time_ms = ((uint32_t)new_section.pulse_count * 100UL) / target->speed_factor;
			if (new_section.unit_time_ms == 0) {
				DG_LAB_ERROR("Error: unit time (%lu ms) must > 0\n", (unsigned long)new_section.unit_time_ms);
				return false;
			}

			new_section.period_start_ms = FREQ_SLIDER_VALUE_MAP[frequency_a];
			new_section.period_end_ms = FREQ_SLIDER_VALUE_MAP[frequency_b];
			const uint32_t target_section_time_ms = (uint32_t)SECTION_TIME_MAP[pulse_section_time] * 100UL;

			new_section.unit_count = (uint16_t)(
				(target_section_time_ms + new_section.unit_time_ms - 1UL) / new_section.unit_time_ms);
			if (new_section.unit_count < 1) {
				new_section.unit_count = 1;
			}
			new_section.section_time_ms = (uint32_t)new_section.unit_count * new_section.unit_time_ms;
			if (new_section.period_start_ms + new_section.period_end_ms >= new_section.section_time_ms) {
				DG_LAB_ERROR(
					"Error: frequency A time (index %ld, %u ms) + frequency B time (index %ld, %u ms) must < section time (%lu ms)\n",
					frequency_a, new_section.period_start_ms, frequency_b, new_section.period_end_ms,
					(unsigned long)new_section.section_time_ms);
				return false;
			}

			if (target->section_count < WAVEFORM_MAX_SECTION_COUNT) {
				new_section.pulses = &target->pulses_pool[pool_index];
				pool_index += new_section.pulse_count;
				target->sections[target->section_count] = new_section;
				target->section_count++;
			} else {
				DG_LAB_ERROR("Error: section count (%u) must < WAVEFORM_MAX_SECTION_COUNT (%d)\n",
					target->section_count, WAVEFORM_MAX_SECTION_COUNT);
				return false;
			}
		}

		// 检查下一小节
		if (*p != '\0') {
			const char *sep = "+section+";
			const size_t sep_len = 9;
			if (strncmp(p, sep, sep_len) != 0) {
				DG_LAB_ERROR("Error: missing \"+section+\" separator\n");
				return false;
			}
			p += sep_len;
			if (*p == '\0') {
				DG_LAB_ERROR("Error: missing section after \"+section+\" separator\n");
				return false;
			}
		}
	}

	if (target->section_count == 0) {
		DG_LAB_ERROR("Error: enabled section count (%u) must > 0\n", target->section_count);
		return false;
	}
	return true;
}
#endif
