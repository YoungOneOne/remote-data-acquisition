/* Host-side regression test links the actual new_display app_proto.c. */
#include "app_proto.h"
#include "app_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char packet[1513];
static wave_window_t wave;
static unsigned wave_count, text_count, seen_timebases;
ui_state_t g_ui;
static unsigned tick;
static unsigned char rx_queue[16000];
static unsigned rx_size, rx_pos;
static char tx_command[128];
uint32_t HAL_GetTick(void) { return tick; }
void HAL_Delay(uint32_t delay) { tick += delay; }
void ui_force_redraw(void) {}
uint16_t drv_uart_read(uint8_t *out, uint16_t max) {
    unsigned n=rx_size-rx_pos;if(n>max)n=max;
    memcpy(out,rx_queue+rx_pos,n);rx_pos+=n;return (uint16_t)n;
}
void drv_uart_send(const uint8_t *data, uint16_t len) {
    assert(len<sizeof tx_command);memcpy(tx_command,data,len);tx_command[len]=0;
}
static void queue_rx(const void *data,unsigned len) {
    assert(rx_size+len<=sizeof rx_queue);memcpy(rx_queue+rx_size,data,len);rx_size+=len;
}
static void queue_reset(void) { rx_pos=rx_size=0; }

static void checksum(void) {
    unsigned char sum=0;
    for(unsigned i=2;i<1510;i++)sum^=packet[i];
    packet[1510]=sum;
}
static void make_frame(unsigned tb) {
    memset(packet,0,sizeof packet);
    packet[0]=0xAA;packet[1]=0x55;packet[2]=2;packet[3]=(unsigned char)tb;
    packet[4]=0xF4;packet[5]=1;packet[6]=0xAA;packet[7]=0x55;packet[8]=2;
    for(unsigned i=0;i<500;i++) {
        packet[10+3*i]=(unsigned char)i;
        packet[11+3*i]=(unsigned char)(255-(i%256));
        packet[12+3*i]=(unsigned char)(i&7);
    }
    packet[1511]=13;packet[1512]=10;checksum();
}
static void drain(void) {
    proto_res_t r;
    for(unsigned guard=0;guard<8192;guard++) {
        r=proto_poll();
        if(r==PROTO_RES_NONE)return;
        if(r==PROTO_RES_WAVE) {
            assert(proto_get_wave(&wave));wave_count++;seen_timebases |= 1u << wave.scan_code;
            assert(wave.n==500);
            assert(wave.ch1[0]==0&&wave.ch2[0]==4095);
            assert(wave.dig[499]==(499&7));
            assert(wave.ch1[499]==((499%256)*4095u+127u)/255u);
        }else text_count++;
    }
    assert(!"parser failed to make progress");
}
static void feed(const unsigned char *p,unsigned len,unsigned chunk) {
    while(len) {
        unsigned n=len<chunk?len:chunk;
        proto_feed(p,(uint16_t)n);drain();p+=n;len-=n;
    }
}
int main(int argc,char **argv) {
    if(argc>1) {
        unsigned char bytes[37];
        FILE *file=fopen(argv[1],"rb"); assert(file);
        proto_init();wave_count=seen_timebases=0;
        size_t n;
        while((n=fread(bytes,1,sizeof bytes,file))!=0) feed(bytes,(unsigned)n,7);
        fclose(file);
        assert(wave_count==5 && seen_timebases==31);
        assert(wave.scan_code==4 && wave.dt_ns==2000);
        puts("PASS: real acquisition DMA packets decoded by real display parser, all five timebases");
    }
    const unsigned dt[5]={200000,100000,20000,10000,2000};
    const unsigned chunks[]={1,7,511,512,1513};
    for(unsigned tb=0;tb<5;tb++)for(unsigned c=0;c<5;c++) {
        proto_init();wave_count=text_count=0;make_frame(tb);
        feed(packet,sizeof packet,chunks[c]);
        assert(wave_count==1&&wave.scan_code==tb&&wave.dt_ns==dt[tb]);
    }
    proto_init();wave_count=0;make_frame(2);
    for(unsigned i=0;i<8;i++)feed(packet,sizeof packet,512);
    assert(wave_count==8);
    /* Corrupt XOR, trailer, count and timebase, then recover on the next frame. */
    for(unsigned fault=0;fault<4;fault++) {
        proto_init();wave_count=0;make_frame(2);
        if(fault==0)packet[100]^=1;
        if(fault==1)packet[1512]=0;
        if(fault==2){packet[4]=0xF5;checksum();}
        if(fault==3){packet[3]=5;checksum();}
        feed(packet,sizeof packet,31);
        assert(wave_count==0);
        make_frame(1);feed(packet,sizeof packet,31);
        assert(wave_count==1&&wave.scan_code==1);
    }
    proto_init();wave_count=text_count=0;
    const char *state="$STATE,TB:4,MODE:1,GEN:1000,RUN:0,TRIG:DIG0(Sound),FALLING,SINGLE,LVL:128,TRGD:1\r\n";
    feed((const unsigned char*)state,(unsigned)strlen(state),3);
    const proto_state_t *st=proto_get_remote_state();
    assert(st->valid&&st->timebase==4&&st->trig_src==3&&st->trig_edge==1&&st->trig_mode==2&&!st->run);
    make_frame(4);feed(packet,sizeof packet,101);assert(wave_count==1);
    char cmd[64];
    for(unsigned i=0;i<5;i++){
        app_link_cmd_trig((uint8_t)i,0,0,2048);
        sprintf(cmd,"TRIG %u 0 0 128\r\nSYNC\r\n",i);
        assert(strcmp(tx_command,cmd)==0);
    }
    /* Exercise the real link manager too, with a hardware-independent UART stub. */
    app_link_init();assert(strcmp(tx_command,"SYNC\r\n")==0);
    queue_reset();make_frame(2);
    for(unsigned i=0;i<8;i++)queue_rx(packet,sizeof packet);
    assert(app_link_poll());assert(g_ui.frame_count==8);
    assert(app_link_get_window(&wave)&&wave.n==500&&wave.scan_code==2);
    app_link_cmd_trig(3,1,2,2048);
    assert(strcmp(tx_command,"TRIG 3 1 2 128\r\nSYNC\r\n")==0);
    g_ui.trig_mode=2;g_ui.run=1;
    queue_reset();queue_rx(packet,sizeof packet);app_link_poll();assert(g_ui.frame_count==8);
    queue_reset();const char *ack="$OK:TRIG=3,1,2,128\r\n";
    queue_rx(ack,(unsigned)strlen(ack));queue_rx(packet,sizeof packet);
    app_link_poll();assert(g_ui.frame_count==9&&!g_ui.run);
    g_ui.trig_mode=0;
    app_link_cmd_arm();assert(strcmp(tx_command,"ARM\r\n")==0&&g_ui.run&&g_ui.trig_mode==2);
    queue_reset();queue_rx(ack,(unsigned)strlen(ack));queue_rx(packet,sizeof packet);
    app_link_poll();assert(g_ui.frame_count==9);
    queue_reset();ack="$OK:ARM_SINGLE\r\n";
    queue_rx(ack,(unsigned)strlen(ack));queue_rx(packet,sizeof packet);
    app_link_poll();assert(g_ui.frame_count==10&&!g_ui.run);
    tick+=1000;assert(!app_link_is_alive());assert(app_link_get_window(&wave));
    /* Actual acquisition key notification updates controls, not frozen data metadata. */
    queue_reset();
    const char *key="[KEY] Switch Timebase to [4]: 100us/div\r\n";
    queue_rx(key,(unsigned)strlen(key));app_link_poll();assert(g_ui.scan==4);
    assert(app_link_get_window(&wave)&&wave.scan_code==2&&wave.dt_ns==20000);
    const char *bad_state="$STATE,TB:1,MODE:1,GEN:1000,RUN:0\r\n";
    queue_reset();queue_rx(bad_state,(unsigned)strlen(bad_state));app_link_poll();
    assert(!proto_get_remote_state()->valid&&g_ui.scan==4);
    const char *output="$OK:GEN_STOP\r\n";
    queue_reset();queue_rx(output,(unsigned)strlen(output));app_link_poll();assert(!g_ui.sqw_on);
    output="$OK:GEN_START\r\n";
    queue_reset();queue_rx(output,(unsigned)strlen(output));app_link_poll();assert(g_ui.sqw_on);
    /* Non-wave replies cannot keep an old waveform marked online forever. */
    assert(!app_link_is_alive());
    /* Sequence wrap, loss, restart and checksum errors. */
    proto_init();
    const uint32_t sequences[]={0xfffffffeu,0xffffffffu,0u,3u,0u};
    for(unsigned i=0;i<5;i++) {
        make_frame(2);
        for(unsigned j=0;j<4;j++)packet[6+j]=(unsigned char)(sequences[i]>>(8*j));
        checksum();feed(packet,sizeof packet,47);
    }
    assert(proto_get_stats()->lost_frames==2);
    make_frame(2);packet[1510]^=1;feed(packet,sizeof packet,47);
    assert(proto_get_stats()->bad_frames>=1);
    puts("PASS: actual KEY/ARM/GEN states, incomplete STATE rejection, sequence loss/wrap/restart and bad-frame stats");
    puts("PASS: real link manager: 8-frame backlog, atomic TRIG ACK, stale ACK rejection, ARM and frozen window");
    puts("PASS: actual new_display C parser: 25 fragmentation cases, consecutive frames,");
    puts("checksum/trailer/count/timebase rejection, recovery, STATE and five source commands.");
    return 0;
}
