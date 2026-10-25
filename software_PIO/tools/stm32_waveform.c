#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
	#include <windows.h>

	// 手动补全 Windows 编码转换所需宏（免除 winnls.h 依赖）
	#ifndef CP_ACP
	#define CP_ACP 0
	#endif
	#ifndef CP_UTF8
	#define CP_UTF8 65001
	#endif

	// 转换为标准 C 基础类型声明的原型，完美避开 TCC 缺失头文件及类型未定义问题
	#ifdef __cplusplus
	extern "C" {
	#endif
	int __stdcall MultiByteToWideChar(
		unsigned int CodePage,
		unsigned long dwFlags,
		const char *lpMultiByteStr,
		int cbMultiByte,
		wchar_t *lpWideCharStr,
		int cchWideChar);
	int __stdcall WideCharToMultiByte(
		unsigned int CodePage,
		unsigned long dwFlags,
		const wchar_t *lpWideCharStr,
		int cchWideChar,
		char *lpMultiByteStr,
		int cbMultiByte,
		const char *lpDefaultChar,
		int *lpUsedDefaultChar);
	#ifdef __cplusplus
	}
	#endif
#else
	#include <dirent.h>
#endif

#include "../include/dg_lab.hpp"

/**
 * @brief Unified output formatter for C headers
 * @param target Parsed waveform object
 * @param raw_str Original raw protocol string
 * @param var_name Target C variable name
 */
static void printStruct(const Waveform_t *target, const char *raw_str, const char *var_name) {
	uint8_t i;
	uint16_t j;
	uint16_t total_pulses = 0;
	for (i = 0; i < target->section_count; i++) {
		total_pulses += target->sections[i].pulse_count;
	}

	// 输出原始内容作为注释
	printf("// %s\n", raw_str);
	// 生成统一的扁平化只读 pulses_pool 数组
	printf("const Pulse_t waveform_%s_pulses_pool[%uU] = {\n\t", var_name, total_pulses);
	for (i = 0; i < target->section_count; i++) {
		const Section_t *sec = &target->sections[i];
		for (j = 0; j < sec->pulse_count; j++) {
			printf("{%uU},", sec->pulses[j].width_us);
		}
		if (i + 1 < target->section_count) {
			printf("\n\t");
		}
	}
	printf("\n};\n");
	// 生成 sections 数组
	printf("const Section_t waveform_%s_sections[%uU] = {\n", var_name, target->section_count);
	uint16_t offset = 0;
	for (i = 0; i < target->section_count; i++) {
		const Section_t *sec = &target->sections[i];
		printf("\t{\n");
		printf("\t\t.pulses          = &waveform_%s_pulses_pool[%uU],\n", var_name, offset);
		printf("\t\t.section_time_ms = %uUL,\n", (unsigned int)sec->section_time_ms);
		printf("\t\t.unit_time_ms    = %uUL,\n", (unsigned int)sec->unit_time_ms);
		printf("\t\t.period_start_ms = %uU,\n", sec->period_start_ms);
		printf("\t\t.period_end_ms   = %uU,\n", sec->period_end_ms);
		printf("\t\t.unit_count      = %uU,\n", sec->unit_count);
		printf("\t\t.pulse_count     = %uU,\n", sec->pulse_count);
		printf("\t\t.frequency_mode  = %uU,\n", sec->frequency_mode);
		printf("\t},\n");
		offset += sec->pulse_count;
	}
	printf("};\n");

	// Output top-level Waveform_t container (Unified layout)
	printf("const Waveform_t waveform_%s = {\n", var_name);
	printf("\t.sections                 = waveform_%s_sections,\n", var_name);
	printf("\t.pulse_section_pause_time = %uU,\n", target->pulse_section_pause_time);
	printf("\t.speed_factor             = %uU,\n", target->speed_factor);
	printf("\t.frequency_balance        = %uU,\n", target->frequency_balance);
	printf("\t.section_count            = %uU,\n", target->section_count);
	printf("};\n\n");
}

#if !defined(_WIN32) && !defined(__WIN32__) && !defined(WIN32)
static bool isPulseFile(const char *filename) {
	size_t len = strlen(filename);
	const char *ext = ".pulse";
	size_t ext_len = strlen(ext);
	if (len <= ext_len) {
		return false;
	}
	return strcmp(filename + len - ext_len, ext) == 0;
}
#endif

/**
 * @brief Get the base filename without path components
 */
static const char *getBaseFilename(const char *path) {
	const char *p1 = strrchr(path, '/');
	const char *p2 = strrchr(path, '\\');
	const char *base = path;
	if (p1 && p1 + 1 > base) base = p1 + 1;
	if (p2 && p2 + 1 > base) base = p2 + 1;
	return base;
}

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
/**
 * @brief Convert local ANSI string (argv) to UTF-8 string on Windows
 */
static void ansiToUtf8(const char *ansi_str, char *utf8_buf, int buf_size) {
	WCHAR wbuf[1024];
	MultiByteToWideChar(CP_ACP, 0, ansi_str, -1, wbuf, 1024);
	WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, utf8_buf, buf_size, NULL, NULL);
}
#endif

/**
 * @brief Process a single .pulse file and output the struct
 * @param display_name UTF-8 encoded filename for comment output
 * @return 0 on success, non-zero error code on failure
 */
static int processFile(const char *file_path, const char *display_name, const char *var_name) {
	FILE *fp = fopen(file_path, "rb");
	if (!fp) {
		fprintf(stderr, "Error: Unable to open file '%s'\n", file_path);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fsize <= 0) {
		fclose(fp);
		fprintf(stderr, "Error: File '%s' is empty\n", file_path);
		return 1;
	}

	char *content = (char *)malloc((size_t)fsize + 1);
	if (!content) {
		fclose(fp);
		return 1;
	}

	size_t read_len = fread(content, 1, (size_t)fsize, fp);
	content[read_len] = '\0';
	fclose(fp);

	// Trim trailing whitespace and newlines
	while (read_len > 0) {
		char c = content[read_len - 1];
		if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
			content[--read_len] = '\0';
		} else {
			break;
		}
	}

	Waveform_t target;
	if (!parseWaveform(&target, content)) {
		fprintf(stderr, "Error: Waveform parsing failed! Please verify protocol content in '%s'.\n", file_path);
		free(content);
		return 2;
	}

	// Output UTF-8 filename metadata comment before structural print
	const char *base_name = getBaseFilename(file_path);
	printf("// %s (%ld bytes)\n", display_name, fsize);

	printStruct(&target, content, var_name);

	free(content);
	return 0;
}

int main(int argc, char *argv[]) {
	const char *input_path = NULL;
	const char *var_name = "preset";
	int positional_idx = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
			input_path = NULL;
			break;
		} else {
			if (positional_idx == 0) {
				input_path = argv[i];
			} else if (positional_idx == 1) {
				var_name = argv[i];
			}
			positional_idx++;
		}
	}

	if (input_path == NULL) {
		fprintf(stderr, "Usage: %s <waveform.pulse|directory> [variable_name] > output.hpp\n", argv[0]);
		return 1;
	}

	struct stat input_stat;
	if (stat(input_path, &input_stat) != 0) {
		fprintf(stderr, "Error: Unable to access path '%s'\n", input_path);
		return 1;
	}

	// Mode 1: Directory processing (non-recursive, all *.pulse files)
	if (S_ISDIR(input_stat.st_mode)) {
		int file_idx = 0;
		char full_path[1024];
		char indexed_var_name[256];

		#define MAX_PRESETS 256
		char var_names_list[MAX_PRESETS][256];

		size_t dir_len = strlen(input_path);
		bool has_slash = (dir_len > 0 && (input_path[dir_len - 1] == '/' || input_path[dir_len - 1] == '\\'));

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
		// Convert ANSI input_path to WideChar for FindFirstFileW
		char search_pattern[1024];
		snprintf(search_pattern, sizeof(search_pattern), "%s%s*.pulse", input_path, has_slash ? "" : "\\");

		WCHAR w_search_pattern[1024];
		MultiByteToWideChar(CP_ACP, 0, search_pattern, -1, w_search_pattern, 1024);

		WIN32_FIND_DATAW find_data;
		HANDLE hFind = FindFirstFileW(w_search_pattern, &find_data);

		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					continue;
				}

				if (file_idx >= MAX_PRESETS) {
					fprintf(stderr, "Warning: Maximum limit of %d presets reached.\n", MAX_PRESETS);
					break;
				}

				// 1. Convert filename to ANSI for fopen() compatibility
				char ansi_filename[512];
				WideCharToMultiByte(CP_ACP, 0, find_data.cFileName, -1, ansi_filename, sizeof(ansi_filename), NULL, NULL);

				// 2. Convert filename to UTF-8 for clean comment output!
				char utf8_filename[512];
				WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1, utf8_filename, sizeof(utf8_filename), NULL, NULL);

				snprintf(full_path, sizeof(full_path), "%s%s%s", input_path, has_slash ? "" : "\\", ansi_filename);
				snprintf(indexed_var_name, sizeof(indexed_var_name), "%s%02d", var_name, file_idx);
				strncpy(var_names_list[file_idx], indexed_var_name, sizeof(var_names_list[file_idx]));

				int ret = processFile(full_path, utf8_filename, indexed_var_name);
				if (ret != 0) {
					FindClose(hFind);
					return ret;
				}

				file_idx++;
			} while (FindNextFileW(hFind, &find_data) != 0);

			FindClose(hFind);
		}
#else
		// POSIX dirent traversal (Linux / macOS - already UTF-8 natively)
		DIR *dir = opendir(input_path);
		if (!dir) {
			fprintf(stderr, "Error: Unable to open directory '%s'\n", input_path);
			return 1;
		}

		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			if (!isPulseFile(entry->d_name)) {
				continue;
			}

			snprintf(full_path, sizeof(full_path), "%s%s%s", input_path, has_slash ? "" : "/", entry->d_name);

			struct stat entry_stat;
			if (stat(full_path, &entry_stat) != 0 || S_ISDIR(entry_stat.st_mode)) {
				continue;
			}

			if (file_idx >= MAX_PRESETS) {
				fprintf(stderr, "Warning: Maximum limit of %d presets reached.\n", MAX_PRESETS);
				break;
			}

			snprintf(indexed_var_name, sizeof(indexed_var_name), "%s%02d", var_name, file_idx);
			strncpy(var_names_list[file_idx], indexed_var_name, sizeof(var_names_list[file_idx]));

			int ret = processFile(full_path, entry->d_name, indexed_var_name);
			if (ret != 0) {
				closedir(dir);
				return ret;
			}

			file_idx++;
		}
		closedir(dir);
#endif

		if (file_idx == 0) {
			fprintf(stderr, "Warning: No '*.pulse' files found in directory '%s'\n", input_path);
			return 1;
		}

		// Output summary pointer list array at the end
		printf("const Waveform_t* const WAVEFORM_LIST[] = {\n");
		for (i = 0; i < file_idx; i++) {
			printf("\t&waveform_%s,\n", var_names_list[i]);
		}
		printf("};\n");

		return 0;
	}

	// Mode 2: Single file processing
	const char *base_name = getBaseFilename(input_path);
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
	char utf8_base_name[512];
	ansiToUtf8(base_name, utf8_base_name, sizeof(utf8_base_name));
	return processFile(input_path, utf8_base_name, var_name);
#else
	return processFile(input_path, base_name, var_name);
#endif
}
