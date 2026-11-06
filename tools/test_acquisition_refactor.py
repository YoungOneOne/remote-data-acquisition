"""Compare real protocol code against a Git baseline with mocked hardware.

Run from a Visual Studio Developer Command Prompt, or let this script locate
the compiler installed on this development machine. No serial port is opened.
"""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'tmp/acquisition-refactor-test'

STUB = r'''
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t ISR, ICR, RDR; } USART_TypeDef;
typedef struct { int Channel, Direction, PeriphInc, MemInc;
 int PeriphDataAlignment, MemDataAlignment, Mode, Priority, FIFOMode;
} DMA_InitTypeDef;
typedef struct { int Instance, State, Lock; DMA_InitTypeDef Init; } DMA_HandleTypeDef;
typedef struct { int gState; DMA_HandleTypeDef *hdmatx; } UART_HandleTypeDef;
extern UART_HandleTypeDef huart1, huart6;
extern USART_TypeDef uart1, uart6;
#define USART1 (&uart1)
#define USART6 (&uart6)
#define HAL_UART_STATE_READY 0
#define HAL_OK 0
#define __HAL_RCC_DMA2_CLK_ENABLE() ((void)0)
#define __HAL_LINKDMA(u, field, dma) ((u)->field = &(dma))
#define __HAL_UART_ENABLE_IT(u, flag) ((void)0)
uint32_t HAL_GetTick(void);
int HAL_UART_Transmit(UART_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
int HAL_UART_Transmit_DMA(UART_HandleTypeDef*, uint8_t*, uint16_t);
int HAL_DMA_Init(DMA_HandleTypeDef*);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef*);
void HAL_NVIC_SetPriority(int, int, int);
void HAL_NVIC_EnableIRQ(int);
void Error_Handler(void);
'''

HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define fputc firmware_fputc
#include "bsp_protocol.c"
#undef fputc
UART_HandleTypeDef huart1, huart6;
USART_TypeDef uart1, uart6;
static TriggerConfig_t cfg = {TRIG_SRC_CH2, TRIG_EDGE_RISING, TRIG_MODE_AUTO, 128, 0};
static Timebase_t tb = TB_1MS;
static uint8_t running = 1, enabled = 1;
static uint32_t frequency = 1000, tick, changes, tx_hash = 2166136261u, tx_calls;
uint32_t HAL_GetTick(void) { return tick; }
int HAL_UART_Transmit(UART_HandleTypeDef *u, uint8_t *b, uint16_t n, uint32_t t) { return 0; }
int HAL_UART_Transmit_DMA(UART_HandleTypeDef *u, uint8_t *b, uint16_t n) {
    assert(n == 1513); assert(b[0] == 0xaa && b[1] == 0x55 && b[2] == 2);
    assert(b[4] == 0xf4 && b[5] == 1 && b[1511] == 13 && b[1512] == 10);
    uint8_t x = 0;
    for (int i = 2; i < 1510; ++i) x ^= b[i];
    assert(x == b[1510]);
    for (int i = 0; i < n; ++i) tx_hash = (tx_hash ^ b[i]) * 16777619u;
    if (u == &huart6) {
        FILE *file = fopen("frames.bin", tx_calls == 0 ? "wb" : "ab");
        assert(file); fwrite(b, 1, n, file); fclose(file);
    }
    tx_calls++; return 0;
}
int HAL_DMA_Init(DMA_HandleTypeDef *d) { return 0; }
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *d) {}
void HAL_NVIC_SetPriority(int a, int b, int c) {}
void HAL_NVIC_EnableIRQ(int a) {}
void Error_Handler(void) { abort(); }
void Acquisition_SetTimebase(Timebase_t t) { tb = t; changes++; }
Timebase_t Acquisition_GetTimebase(void) { return tb; }
void Acquisition_Resume(void) { running = 1; changes++; }
void Acquisition_Pause(void) { running = 0; changes++; }
uint8_t Acquisition_IsRunning(void) { return running; }
void Acquisition_SetTrigger(TrigSource_t s, TrigEdge_t e, TrigMode_t m, uint8_t l) {
    cfg.source=s; cfg.edge=e; cfg.mode=m; cfg.level=l; changes++;
}
void Acquisition_ArmSingleTrigger(void) { cfg.mode=TRIG_MODE_SINGLE; running=1; changes++; }
TriggerConfig_t *Acquisition_GetTriggerConfig(void) { return &cfg; }
const char *Acquisition_GetTimebaseName(Timebase_t t) { return "test-timebase"; }
const char *Acquisition_GetTrigSourceName(TrigSource_t s) {
    static const char *n[] = {"CH1", "CH2", "DIG2", "DIG0", "DIG1"}; return n[s];
}
const char *Acquisition_GetTrigEdgeName(TrigEdge_t e) { return e ? "FALLING" : "RISING"; }
const char *Acquisition_GetTrigModeName(TrigMode_t m) {
    static const char *n[] = {"AUTO", "NORMAL", "SINGLE"}; return n[m];
}
void Generator_Start(void) { enabled=1; changes++; }
void Generator_Stop(void) { enabled=0; changes++; }
uint32_t Generator_GetFrequency(void) { return frequency; }
uint32_t Generator_SetFrequency(uint32_t f) { changes++; return frequency=f; }
uint32_t Generator_NextPreset(void) { changes++; return frequency=2000; }
uint32_t Generator_PrevPreset(void) { changes++; return frequency=500; }
static void execute(const char *s) {
    printf("CMD:%s\n", s);
    Protocol_ExecuteCommand(s);
    printf("STATE:%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
        tb, running, cfg.source, cfg.edge, cfg.mode, cfg.level,
        frequency, enabled, Protocol_GetMode(), changes);
}
int main(void) {
    Protocol_Init();
    const char *commands[] = {
        "", "  tb:4  ", "TB=0", "TB 5", "TB -1", "TB nonsense",
        "GEN_START", "GEN START", "GEN ON", "GEN_STOP", "GEN STOP", "GEN OFF",
        "GEN_NEXT", "GEN_PREV", "GEN=100", "GEN:10000", "GEN 99", "GEN 10001",
        "RUN", "START", "STOP", "PAUSE", "MODE 0", "MODE TXT", "MODE TEXT",
        "MODE BIN", "MODE BINARY", "MODE 1", "MODE BAD",
        "TRIG_MODE AUTO", "TRIG_MODE NORMAL", "TRIG_MODE NORM", "TRIG_MODE SINGLE",
        "TRIG_MODE 0", "TRIG_MODE 1", "TRIG_MODE 2", "TRIG_MODE 3",
        "TRIG_EDGE RISE", "TRIG_EDGE RISING", "TRIG_EDGE FALL", "TRIG_EDGE FALLING",
        "TRIG_EDGE 0", "TRIG_EDGE 1", "TRIG_EDGE 2", "TRIG_LVL 0", "TRIG_LEVEL:255",
        "TRIG_LVL -1", "TRIG_LVL 256", "TRIG_LVL BAD", "ARM", "SINGLE", "SYNC", "STATUS",
        "TRIG 5 0 0 128", "TRIG 0 2 0 128", "TRIG 0 0 3 128", "TRIG 0 0 0 256",
        "TRIG 0 0 0 128 EXTRA", "TRIG 0 0 0", "UNKNOWN"
    };
    for (unsigned i=0; i<sizeof(commands)/sizeof(commands[0]); ++i) execute(commands[i]);
    const char *sources[] = {"CH1", "CH2", "DIG2", "DIG0", "DIG1", "0", "1", "2", "3", "4", "BAD"};
    char cmd[64];
    for (unsigned i=0; i<11; ++i) { sprintf(cmd,"TRIG_SRC %s",sources[i]); execute(cmd); }
    for (int s=0;s<5;s++) for (int e=0;e<2;e++) for (int m=0;m<3;m++) {
        sprintf(cmd,"TRIG %d %d %d %d",s,e,m,(s+e+m)*31); execute(cmd);
    }
    /* Exercise the actual receive queue/line parser, including busy deferral. */
    const char *rx="  tb=3\r\nSYNC\n";
    while (*rx) { s_rx_ring[s_rx_head]=*rx++; s_rx_head=(s_rx_head+1)%UART_RX_RING_SIZE; }
    huart6.gState=1; Protocol_ProcessRx(); assert(s_rx_tail==0);
    huart6.gState=0; Protocol_ProcessRx(); assert(tb==3 && s_rx_head==s_rx_tail);
    s_rx_ring[s_rx_head++]='T'; Protocol_ProcessRx(); tick=101; Protocol_ProcessRx();
    rx="SYNC\n";
    while (*rx) { s_rx_ring[s_rx_head]=*rx++; s_rx_head=(s_rx_head+1)%UART_RX_RING_SIZE; }
    Protocol_ProcessRx();
    AcqPoint_t points[ACQ_FRAME_POINTS];
    for (int i=0;i<ACQ_FRAME_POINTS;i++) {
        points[i].ch1_raw8=(uint8_t)i; points[i].ch2_raw8=(uint8_t)(255-i); points[i].dig_mask=i&7;
    }
    for (int t=0;t<5;t++) Protocol_SendBinaryFrame((Timebase_t)t, points, ACQ_FRAME_POINTS);
    assert(tx_calls==10);
    huart1.gState=1; Protocol_SendBinaryFrame(TB_1MS, points, ACQ_FRAME_POINTS); huart1.gState=0;
    Protocol_SendBinaryFrame(TB_1MS, points, 499); assert(tx_calls==10);
    printf("PACKETS:%u,%08x\n",tx_calls,tx_hash);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', default='11212f4')
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    def baseline(path):
        return subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=ROOT).decode('utf-8-sig')
    constants = '''DMA2_Stream6 DMA_CHANNEL_5 DMA_MEMORY_TO_PERIPH DMA_PINC_DISABLE
        DMA_MINC_ENABLE DMA_PDATAALIGN_BYTE DMA_MDATAALIGN_BYTE DMA_NORMAL DMA_PRIORITY_LOW
        DMA_FIFOMODE_DISABLE DMA2_Stream6_IRQn DMA2_Stream7 DMA_CHANNEL_4 HAL_DMA_STATE_RESET
        HAL_UNLOCKED DMA2_Stream7_IRQn USART1_IRQn USART6_IRQn UART_IT_RXNE UART_IT_ERR'''.split()
    flags = 'USART_ISR_ORE USART_ISR_FE USART_ISR_NE USART_ISR_RXNE USART_ICR_ORECF USART_ICR_FECF USART_ICR_NCF'.split()
    header = STUB + ''.join(f'\n#define {c} {i+1}' for i,c in enumerate(constants))
    header += ''.join(f'\n#define {c} {1<<i}' for i,c in enumerate(flags)) + '\n'
    header += (ROOT/'tools/acquisition_host/algorithm_hal.h').read_text(encoding='utf-8')
    for version in ['before', 'after']:
        directory = WORK/version
        directory.mkdir(exist_ok=True)
        (directory/'main.h').write_text(header)
        for name in ['adc.h', 'tim.h', 'usart.h']:
            (directory/name).write_text('#include "main.h"\n')
        for name in ['bsp_protocol.h', 'bsp_acquisition.h', 'bsp_generator.h']:
            header_text = (ROOT/'Acquisition/Inc'/name).read_text(encoding='utf-8-sig')
            header_text = header_text.replace('../../Common/scope_protocol.h', (ROOT/'Common/scope_protocol.h').as_posix())
            (directory/name).write_text(header_text, encoding='utf-8')
        for name in ['bsp_protocol.c', 'bsp_acquisition.c', 'bsp_generator.c']:
            path = f'Acquisition/Src/{name}'
            code = baseline(path) if version == 'before' else (ROOT/path).read_text(encoding='utf-8-sig')
            (directory/name).write_text(code, encoding='utf-8')
        (directory/'test.c').write_text(HARNESS, encoding='utf-8')
        for name in ['test_acquisition_algorithms.c', 'test_generator_algorithms.c']:
            (directory/name).write_text((ROOT/'tools'/name).read_text(encoding='utf-8'), encoding='utf-8')
        vcvars = r'D:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat'
        batch = directory/'build.bat'
        commands = [f'@echo off', f'call "{vcvars}" >nul', 'if errorlevel 1 exit /b 1']
        for source, program in [('test.c', 'test'), ('test_acquisition_algorithms.c', 'acquisition'),
                                ('test_generator_algorithms.c', 'generator')]:
            commands += [f'cl /nologo /w /utf-8 /std:c11 /D_CRT_SECURE_NO_WARNINGS {source} /Fe:{program}.exe /link /INCREMENTAL:NO',
                         'if errorlevel 1 exit /b 1']
        batch.write_text('\n'.join(commands) + '\n')
        result = subprocess.run(['cmd', '/c', str(batch)], cwd=directory, capture_output=True)
        assert result.returncode == 0, result.stdout.decode(errors='replace')+result.stderr.decode(errors='replace')
        output = subprocess.check_output([str(directory/'test.exe')], cwd=directory)
        (directory/'result.txt').write_bytes(output)
        for program in ['acquisition', 'generator']:
            output = subprocess.check_output([str(directory/f'{program}.exe')], cwd=directory)
            (directory/f'{program}-result.txt').write_bytes(output)
    before = (WORK/'before/result.txt').read_bytes()
    after = (WORK/'after/result.txt').read_bytes()
    assert before == after, 'Protocol replies/state/packets differ; inspect tmp/acquisition-refactor-test'
    print(f'PASS: {before.count(b"CMD:")} command cases, queue/busy/timeout behavior and five timebase packets match baseline')
    for program in ['acquisition', 'generator']:
        before = (WORK/f'before/{program}-result.txt').read_bytes()
        after = (WORK/f'after/{program}-result.txt').read_bytes()
        assert before == after, f'{program} behavior differs from baseline'
        print(after.decode().strip())


if __name__ == '__main__':
    main()
