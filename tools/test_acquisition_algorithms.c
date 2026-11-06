/* Compile this against both the Git baseline and the edited acquisition source. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "bsp_acquisition.c"

TIM_TypeDef test_timer2, test_timer3;
TIM_HandleTypeDef htim2 = {&test_timer2}, htim3 = {&test_timer3};
GPIO_TypeDef test_gpiof, test_gpiog, test_gpioi;
ADC_HandleTypeDef hadc3 = {ADC3};
static unsigned starts, stops;
static uint8_t invalidate_snapshot;
static uint32_t frame_hash = 2166136261u;

void test_memory_barrier(void) {
    if (invalidate_snapshot) { s_history_version += 2; invalidate_snapshot = 0; }
}
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config) {}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, int value) {
    if (value) port->ODR |= pin; else port->ODR &= ~pin;
}
int HAL_ADC_Start_DMA(ADC_HandleTypeDef *adc, uint32_t *buffer, uint32_t count) {
    assert(adc == &hadc3 && count == 2000); starts++; return 0;
}
int HAL_ADC_Stop_DMA(ADC_HandleTypeDef *adc) { stops++; return 0; }
int HAL_TIM_Base_Start(TIM_HandleTypeDef *timer) { return 0; }
int HAL_TIM_Base_Stop(TIM_HandleTypeDef *timer) { return 0; }
void HAL_NVIC_SetPriority(int irq, int priority, int subpriority) {}
void HAL_NVIC_EnableIRQ(int irq) {}

static void test_edges(void)
{
    const int levels[] = {0, 1, 127, 128, 254, 255};
    const int bits[] = {0, 0, 2, 0, 1};
    for (int src = 0; src < 5; src++) for (int edge = 0; edge < 2; edge++)
    for (int l = 0; l < 6; l++) for (int a = 0; a < 256; a++) for (int b = 0; b < 256; b++) {
        int level = levels[l], expected;
        s_trig_cfg.source = (TrigSource_t)src;
        s_trig_cfg.edge = (TrigEdge_t)edge;
        s_trig_cfg.level = (uint8_t)level;
        /* Unselected channels vary independently to catch source mapping mistakes. */
        s_snapshot_adc[0] = (uint16_t)(a << 4);
        s_snapshot_adc[2] = (uint16_t)(b << 4);
        s_snapshot_adc[1] = (uint16_t)((255 - a) << 4);
        s_snapshot_adc[3] = (uint16_t)((255 - b) << 4);
        s_snapshot_dig[0] = (uint8_t)a;
        s_snapshot_dig[1] = (uint8_t)b;
        int prev = src == 1 ? 255 - a : a;
        int curr = src == 1 ? 255 - b : b;
        if (src >= 2) {
            prev = (a >> bits[src]) & 1;
            curr = (b >> bits[src]) & 1;
            expected = edge ? prev == 1 && curr == 0 : prev == 0 && curr == 1;
        } else {
            expected = edge ? prev > level && curr <= level : prev < level && curr >= level;
        }
        assert(Check_Trigger_Edge(0, 1) == expected);
    }
    puts("PASS: five trigger sources, both edges and boundary levels (3,932,160 comparisons)");
}

static void seed_history(int src, int edge, int position)
{
    const int bits[] = {0, 0, 2, 0, 1};
    for (int i = 0; i < SAMPLES_PER_CH; i++) {
        int high = edge ? i < position : i >= position;
        s_history_adc[i * 2] = 2048;
        s_history_adc[i * 2 + 1] = 2048;
        s_history_dig[i] = 0;
        if (src < 2) s_history_adc[i * 2 + src] = high ? 3072 : 1024;
        else s_history_dig[i] = (uint8_t)(high << bits[src]);
    }
    s_history_version = 2;
}

static void test_frames(void)
{
    AcqPoint_t points[ACQ_FRAME_POINTS];
    const int positions[] = {1, 123, 500, 501};
    for (int tb = 0; tb < 5; tb++) for (int src = 0; src < 5; src++)
    for (int edge = 0; edge < 2; edge++) for (int mode = 0; mode < 3; mode++)
    for (int p = 0; p < 4; p++) {
        float v1, v2, vpp;
        uint8_t digital;
        int position = positions[p], found = position <= 500;
        Acquisition_SetTimebase((Timebase_t)tb);
        Acquisition_SetTrigger((TrigSource_t)src, (TrigEdge_t)edge, (TrigMode_t)mode, 128);
        Acquisition_Resume();
        seed_history(src, edge, position);
        int ok = Acquisition_GetFrame(points, &v1, &v2, &vpp, &digital);
        assert(ok == (found || mode == TRIG_MODE_AUTO));
        assert(s_trig_cfg.triggered == found);
        assert(((test_gpiog.ODR & GPIO_PIN_7) != 0) == found);
        if (!ok) continue;
        assert(Acquisition_GetFrameTimebase() == tb);
        assert(Acquisition_IsRunning() == (mode != TRIG_MODE_SINGLE));
        uint32_t sum1 = 0, sum2 = 0;
        uint16_t minimum = 4095, maximum = 0;
        int start = found ? position : 0;
        for (int i = 0; i < ACQ_FRAME_POINTS; i++) {
            int index = start + i;
            uint16_t raw1 = s_history_adc[index * 2], raw2 = s_history_adc[index * 2 + 1];
            assert(points[i].ch1_raw8 == raw1 >> 4 && points[i].ch2_raw8 == raw2 >> 4);
            assert(points[i].dig_mask == s_history_dig[index]);
            sum1 += raw1; sum2 += raw2;
            if (raw2 < minimum) minimum = raw2;
            if (raw2 > maximum) maximum = raw2;
            frame_hash = (frame_hash ^ points[i].ch1_raw8) * 16777619u;
            frame_hash = (frame_hash ^ points[i].ch2_raw8) * 16777619u;
            frame_hash = (frame_hash ^ points[i].dig_mask) * 16777619u;
        }
        assert(fabsf(v1 - sum1 / 500.0f * 3.3f / 4095) < 0.00001f);
        assert(fabsf(v2 - sum2 / 500.0f * 3.3f / 4095) < 0.00001f);
        assert(fabsf(vpp - (maximum - minimum) * 3.3f / 4095) < 0.00001f);
        assert(digital == points[499].dig_mask);
    }
    seed_history(0, 0, 123);
    s_history_version = 3;
    assert(!Acquisition_GetFrame(points, NULL, NULL, NULL, NULL));
    s_history_version = 4;
    invalidate_snapshot = 1;
    assert(!Acquisition_GetFrame(points, NULL, NULL, NULL, NULL));
    printf("PASS: 600 timebase/source/edge/mode/window cases, metrics and interrupted snapshots; hash=%08x\n", frame_hash);
}

static void test_dma_and_control(void)
{
    Acquisition_Init();
    assert(s_initialized && test_timer2.ARR == 19 && starts == 1);
    for (int mask = 0; mask < 8; mask++) {
        test_gpioi.IDR = (uint32_t)(mask & 5);
        test_gpiof.IDR = (mask & 2) ? GPIO_PIN_8 : 0;
        test_timer2.SR = TIM_SR_UIF;
        TIM2_IRQHandler();
        assert(s_dig_buffer[mask] == mask);
    }
    s_dig_sample_idx = SAMPLES_PER_CH - 1;
    test_timer2.SR = TIM_SR_UIF;
    TIM2_IRQHandler();
    assert(s_dig_sample_idx == 0);

    for (int i = 0; i < ADC_DMA_BUF_LEN; i++) s_adc_dma_buffer[i] = (uint16_t)i;
    for (int i = 0; i < SAMPLES_PER_CH; i++) s_dig_buffer[i] = (uint8_t)(i & 7);
    HAL_ADC_ConvHalfCpltCallback(&hadc3);
    assert(!Acquisition_IsFrameReady());
    HAL_ADC_ConvCpltCallback(&hadc3);
    assert(Acquisition_IsFrameReady() && !Acquisition_IsFrameReady());
    for (int i = 0; i < ADC_DMA_BUF_LEN; i++) assert(s_history_adc[i] == i);
    for (int i = 0; i < SAMPLES_PER_CH; i++) assert(s_history_dig[i] == (i & 7));

    const uint32_t arr[] = SCOPE_TIM2_ARR;
    for (int tb = 0; tb < 5; tb++) {
        Acquisition_SetTimebase((Timebase_t)tb);
        assert(test_timer2.ARR == arr[tb] && Acquisition_GetTimebase() == tb);
        assert(s_dig_sample_idx == 0 && s_history_blocks == 0 && !Acquisition_IsFrameReady());
    }
    Acquisition_Pause(); assert(!Acquisition_IsRunning());
    Acquisition_Resume(); assert(Acquisition_IsRunning());
    Acquisition_ArmSingleTrigger();
    assert(Acquisition_IsRunning() && s_trig_cfg.mode == TRIG_MODE_SINGLE);
    uint32_t errors = Acquisition_GetOvrErrorCount();
    HAL_ADC_ErrorCallback(&hadc3);
    assert(Acquisition_GetOvrErrorCount() == errors + 1 && s_history_blocks == 0);
    assert(starts == stops + 1);
    puts("PASS: digital sampling, DMA halves, five timebases, pause/resume/ARM and overrun recovery");
}

int main(void)
{
    test_dma_and_control();
    test_edges();
    test_frames();
    return 0;
}
