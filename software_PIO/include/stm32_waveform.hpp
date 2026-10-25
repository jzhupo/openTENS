// 乳头.pulse (198 bytes)
// Dungeonlab+pulse:63,1,16=0,32,4,2,1/0.00-1,50.00-0,100.00-1,100.00-1,100.00-0,100.00-0,100.00-1,92.01-0,84.03-0,76.04-0,68.06-1+section+0,20,8,1,1/52.55-1,100.00-1+section+0,20,8,1,0/0.00-1,100.00-1
const Pulse_t waveform_00_pulses_pool[13U] = {
	{0U},{75U},{150U},{150U},{150U},{150U},{150U},{138U},{126U},{114U},{102U},
	{78U},{150U},
};
const Section_t waveform_00_sections[2U] = {
	{
		.pulses          = &waveform_00_pulses_pool[0U],
		.section_time_ms = 1100UL,
		.unit_time_ms    = 1100UL,
		.period_start_ms = 10U,
		.period_end_ms   = 42U,
		.unit_count      = 1U,
		.pulse_count     = 11U,
		.frequency_mode  = 2U,
	},
	{
		.pulses          = &waveform_00_pulses_pool[11U],
		.section_time_ms = 1000UL,
		.unit_time_ms    = 200UL,
		.period_start_ms = 10U,
		.period_end_ms   = 30U,
		.unit_count      = 5U,
		.pulse_count     = 2U,
		.frequency_mode  = 1U,
	},
};
const Waveform_t waveform_00 = {
	.sections                 = waveform_00_sections,
	.pulse_section_pause_time = 7U,
	.speed_factor             = 1U,
	.frequency_balance        = 16U,
	.section_count            = 2U,
};

// 乳夹.pulse (303 bytes)
// Dungeonlab+pulse:0,1,16=27,7,17,3,1/1.43-1,34.28-0,67.14-0,100.00-1,50.00-0,0.00-1,100.00-1+section+0,20,23,2,1/6.33-1,54.26-1,27.13-0,0.00-1,13.43-0,26.86-0,40.29-0,53.72-0,67.15-1+section+0,20,8,1,1/0.00-1,5.26-0,10.51-0,15.77-0,21.02-0,26.28-1,36.81-0,47.34-0,57.87-0,68.40-0,78.94-0,89.47-0,100.00-1
const Pulse_t waveform_01_pulses_pool[29U] = {
	{2U},{51U},{100U},{150U},{75U},{0U},{150U},
	{9U},{81U},{40U},{0U},{20U},{40U},{60U},{80U},{100U},
	{0U},{7U},{15U},{23U},{31U},{39U},{55U},{71U},{86U},{102U},{118U},{134U},{150U},
};
const Section_t waveform_01_sections[3U] = {
	{
		.pulses          = &waveform_01_pulses_pool[0U],
		.section_time_ms = 2100UL,
		.unit_time_ms    = 700UL,
		.period_start_ms = 37U,
		.period_end_ms   = 17U,
		.unit_count      = 3U,
		.pulse_count     = 7U,
		.frequency_mode  = 3U,
	},
	{
		.pulses          = &waveform_01_pulses_pool[7U],
		.section_time_ms = 2700UL,
		.unit_time_ms    = 900UL,
		.period_start_ms = 10U,
		.period_end_ms   = 30U,
		.unit_count      = 3U,
		.pulse_count     = 9U,
		.frequency_mode  = 2U,
	},
	{
		.pulses          = &waveform_01_pulses_pool[16U],
		.section_time_ms = 1300UL,
		.unit_time_ms    = 1300UL,
		.period_start_ms = 10U,
		.period_end_ms   = 30U,
		.unit_count      = 1U,
		.pulse_count     = 13U,
		.frequency_mode  = 1U,
	},
};
const Waveform_t waveform_01 = {
	.sections                 = waveform_01_sections,
	.pulse_section_pause_time = 0U,
	.speed_factor             = 1U,
	.frequency_balance        = 16U,
	.section_count            = 3U,
};

// 后穴挑逗.pulse (203 bytes)
// Dungeonlab+pulse:5,1,16=0,20,20,3,1/0.00-1,100.00-1,100.00-1,100.00-1,0.00-1+section+1,20,9,1,1/100.00-1,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-1+section+0,20,8,3,1/89.05-1,100.00-1
const Pulse_t waveform_02_pulses_pool[15U] = {
	{0U},{150U},{150U},{150U},{0U},
	{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},
	{133U},{150U},
};
const Section_t waveform_02_sections[3U] = {
	{
		.pulses          = &waveform_02_pulses_pool[0U],
		.section_time_ms = 2500UL,
		.unit_time_ms    = 500UL,
		.period_start_ms = 10U,
		.period_end_ms   = 30U,
		.unit_count      = 5U,
		.pulse_count     = 5U,
		.frequency_mode  = 3U,
	},
	{
		.pulses          = &waveform_02_pulses_pool[5U],
		.section_time_ms = 1600UL,
		.unit_time_ms    = 800UL,
		.period_start_ms = 11U,
		.period_end_ms   = 30U,
		.unit_count      = 2U,
		.pulse_count     = 8U,
		.frequency_mode  = 1U,
	},
	{
		.pulses          = &waveform_02_pulses_pool[13U],
		.section_time_ms = 1000UL,
		.unit_time_ms    = 200UL,
		.period_start_ms = 10U,
		.period_end_ms   = 30U,
		.unit_count      = 5U,
		.pulse_count     = 2U,
		.frequency_mode  = 3U,
	},
};
const Waveform_t waveform_02 = {
	.sections                 = waveform_02_sections,
	.pulse_section_pause_time = 1U,
	.speed_factor             = 1U,
	.frequency_balance        = 16U,
	.section_count            = 3U,
};

// 天使的抚摸.pulse (415 bytes)
// Dungeonlab+pulse:0,1,16=4,16,21,1,1/39.80-1,20.15-1,41.55-1,20.80-1,45.00-1,30.00-1,55.00-1,35.00-1,60.00-1,45.00-1,65.00-1,55.00-1,70.00-1,60.00-1,80.00-1,65.00-1,95.00-1,75.00-1,80.00-1,50.00-1,60.00-1,30.00-1,45.00-1,20.00-1,35.00-1,50.00-1,65.00-1,45.00-1,24.65-1+section+10,20,56,1,1/42.45-1,55.45-1,65.10-1,75.70-1,60.40-1,49.25-0,38.15-1,29.15-1+section+15,20,34,1,1/0.00-1,100.00-1,50.10-1,100.00-1,100.00-1
const Pulse_t waveform_03_pulses_pool[42U] = {
	{59U},{30U},{62U},{31U},{67U},{45U},{82U},{52U},{90U},{67U},{97U},{82U},{105U},{90U},{120U},{97U},{142U},{112U},{120U},{75U},{90U},{45U},{67U},{30U},{52U},{75U},{97U},{67U},{36U},
	{63U},{83U},{97U},{113U},{90U},{73U},{57U},{43U},
	{0U},{150U},{75U},{150U},{150U},
};
const Section_t waveform_03_sections[3U] = {
	{
		.pulses          = &waveform_03_pulses_pool[0U],
		.section_time_ms = 2900UL,
		.unit_time_ms    = 2900UL,
		.period_start_ms = 14U,
		.period_end_ms   = 26U,
		.unit_count      = 1U,
		.pulse_count     = 29U,
		.frequency_mode  = 1U,
	},
	{
		.pulses          = &waveform_03_pulses_pool[29U],
		.section_time_ms = 6400UL,
		.unit_time_ms    = 800UL,
		.period_start_ms = 20U,
		.period_end_ms   = 30U,
		.unit_count      = 8U,
		.pulse_count     = 8U,
		.frequency_mode  = 1U,
	},
	{
		.pulses          = &waveform_03_pulses_pool[37U],
		.section_time_ms = 3500UL,
		.unit_time_ms    = 500UL,
		.period_start_ms = 25U,
		.period_end_ms   = 30U,
		.unit_count      = 7U,
		.pulse_count     = 5U,
		.frequency_mode  = 1U,
	},
};
const Waveform_t waveform_03 = {
	.sections                 = waveform_03_sections,
	.pulse_section_pause_time = 0U,
	.speed_factor             = 1U,
	.frequency_balance        = 16U,
	.section_count            = 3U,
};

// 攥紧.pulse (526 bytes)
// Dungeonlab+pulse:0,1,16=0,26,0,2,1/0.00-1,20.00-0,40.00-0,60.00-0,80.00-0,100.00-1,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-1+section+37,0,19,2,1/0.00-1,100.00-1+section+36,0,23,2,1/0.00-1,100.00-1,100.00-1,0.00-1+section+0,20,19,1,1/0.00-1,100.00-1,100.00-1,0.00-1+section+0,20,19,1,1/100.00-1,100.00-1+section+0,20,20,1,0/0.00-1,20.00-1+section+0,20,20,1,0/0.00-1,20.00-1+section+0,20,20,1,0/0.00-1,20.00-1+section+0,20,20,1,0/0.00-1,20.00-1
const Pulse_t waveform_04_pulses_pool[32U] = {
	{0U},{30U},{60U},{90U},{120U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},
	{0U},{150U},
	{0U},{150U},{150U},{0U},
	{0U},{150U},{150U},{0U},
	{150U},{150U},
};
const Section_t waveform_04_sections[5U] = {
	{
		.pulses          = &waveform_04_pulses_pool[0U],
		.section_time_ms = 2000UL,
		.unit_time_ms    = 2000UL,
		.period_start_ms = 10U,
		.period_end_ms   = 36U,
		.unit_count      = 1U,
		.pulse_count     = 20U,
		.frequency_mode  = 2U,
	},
	{
		.pulses          = &waveform_04_pulses_pool[20U],
		.section_time_ms = 2000UL,
		.unit_time_ms    = 200UL,
		.period_start_ms = 47U,
		.period_end_ms   = 10U,
		.unit_count      = 10U,
		.pulse_count     = 2U,
		.frequency_mode  = 2U,
	},
	{
		.pulses          = &waveform_04_pulses_pool[22U],
		.section_time_ms = 2400UL,
		.unit_time_ms    = 400UL,
		.period_start_ms = 46U,
		.period_end_ms   = 10U,
		.unit_count      = 6U,
		.pulse_count     = 4U,
		.frequency_mode  = 2U,
	},
	{
		.pulses          = &waveform_04_pulses_pool[26U],
		.section_time_ms = 2000UL,
		.unit_time_ms    = 400UL,
		.period_start_ms = 10U,
		.period_end_ms   = 30U,
		.unit_count      = 5U,
		.pulse_count     = 4U,
		.frequency_mode  = 1U,
	},
	{
		.pulses          = &waveform_04_pulses_pool[30U],
		.section_time_ms = 2000UL,
		.unit_time_ms    = 200UL,
		.period_start_ms = 10U,
		.period_end_ms   = 30U,
		.unit_count      = 10U,
		.pulse_count     = 2U,
		.frequency_mode  = 1U,
	},
};
const Waveform_t waveform_04 = {
	.sections                 = waveform_04_sections,
	.pulse_section_pause_time = 0U,
	.speed_factor             = 1U,
	.frequency_balance        = 16U,
	.section_count            = 5U,
};

// 毒龙钻.pulse (122 bytes)
// Dungeonlab+pulse:3,1,16=55,20,5,4,1/100.00-1,100.00-1+section+0,0,14,1,1/40.00-1,60.00-0,80.00-0,100.00-1,30.00-1,100.00-1
const Pulse_t waveform_05_pulses_pool[8U] = {
	{150U},{150U},
	{60U},{90U},{120U},{150U},{45U},{150U},
};
const Section_t waveform_05_sections[2U] = {
	{
		.pulses          = &waveform_05_pulses_pool[0U],
		.section_time_ms = 600UL,
		.unit_time_ms    = 200UL,
		.period_start_ms = 80U,
		.period_end_ms   = 30U,
		.unit_count      = 3U,
		.pulse_count     = 2U,
		.frequency_mode  = 4U,
	},
	{
		.pulses          = &waveform_05_pulses_pool[2U],
		.section_time_ms = 1800UL,
		.unit_time_ms    = 600UL,
		.period_start_ms = 10U,
		.period_end_ms   = 10U,
		.unit_count      = 3U,
		.pulse_count     = 6U,
		.frequency_mode  = 1U,
	},
};
const Waveform_t waveform_05 = {
	.sections                 = waveform_05_sections,
	.pulse_section_pause_time = 1U,
	.speed_factor             = 1U,
	.frequency_balance        = 16U,
	.section_count            = 2U,
};

// 辞别的心跳.pulse (857 bytes)
// Dungeonlab+pulse:0,1,1=10,23,0,1,1/45.00-1,100.00-1,50.00-0,0.00-1,0.00-0,0.00-1,0.00-1,50.00-0,100.00-1,50.00-0,0.00-1,0.00-0,0.00-1,45.00-1,100.00-1,40.00-1,0.00-1,40.00-1,90.00-1,40.00-1,0.00-1,100.00-1,50.00-0,0.00-1,50.00-0,100.00-1,50.00-0,0.00-1,50.00-0,100.00-1,45.00-1,45.00-1,100.00-1,50.00-0,0.00-1,0.00-0,0.00-0,0.00-0,0.00-0,0.00-1,25.00-1,55.00-1,75.00-1,90.00-1,73.33-0,56.67-0,40.00-1,45.00-0,50.00-1,0.00-1,50.00-0,100.00-1,50.00-0,0.00-1,100.00-1,0.00-1,50.00-0,100.00-1,50.00-0,0.00-1,100.00-1,0.00-1,10.00-0,20.00-1,33.33-0,46.67-0,60.00-0,73.33-0,86.67-0,100.00-1,100.00-0,100.00-0,100.00-0,100.00-0,100.00-0,100.00-1,0.00-1,1.25-0,2.50-0,3.75-0,5.00-1,10.00-0,15.00-0,20.00-0,25.00-1,22.22-0,19.44-0,16.67-0,13.89-0,11.11-0,8.33-0,5.56-0,2.78-0,0.00-1,0.00-0,0.00-0,0.00-0,0.00-0,0.00-0,0.00-0,0.00-0,0.00-0,0.00-0,0.00-0,0.00-0,0.00-1
const Pulse_t waveform_06_pulses_pool[106U] = {
	{67U},{150U},{75U},{0U},{0U},{0U},{0U},{75U},{150U},{75U},{0U},{0U},{0U},{67U},{150U},{60U},{0U},{60U},{135U},{60U},{0U},{150U},{75U},{0U},{75U},{150U},{75U},{0U},{75U},{150U},{67U},{67U},{150U},{75U},{0U},{0U},{0U},{0U},{0U},{0U},{37U},{82U},{112U},{135U},{109U},{85U},{60U},{67U},{75U},{0U},{75U},{150U},{75U},{0U},{150U},{0U},{75U},{150U},{75U},{0U},{150U},{0U},{15U},{30U},{49U},{70U},{90U},{109U},{130U},{150U},{150U},{150U},{150U},{150U},{150U},{150U},{0U},{1U},{3U},{5U},{7U},{15U},{22U},{30U},{37U},{33U},{29U},{25U},{20U},{16U},{12U},{8U},{4U},{0U},{0U},{0U},{0U},{0U},{0U},{0U},{0U},{0U},{0U},{0U},{0U},{0U},
};
const Section_t waveform_06_sections[1U] = {
	{
		.pulses          = &waveform_06_pulses_pool[0U],
		.section_time_ms = 10600UL,
		.unit_time_ms    = 10600UL,
		.period_start_ms = 20U,
		.period_end_ms   = 33U,
		.unit_count      = 1U,
		.pulse_count     = 106U,
		.frequency_mode  = 1U,
	},
};
const Waveform_t waveform_06 = {
	.sections                 = waveform_06_sections,
	.pulse_section_pause_time = 0U,
	.speed_factor             = 1U,
	.frequency_balance        = 1U,
	.section_count            = 1U,
};

const Waveform_t* const WAVEFORM_LIST[] = {
	&waveform_00,
	&waveform_01,
	&waveform_02,
	&waveform_03,
	&waveform_04,
	&waveform_05,
	&waveform_06,
};
