/* Run the actual framebuffer renderer with desktop LCD/UART substitutes. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../new_display/quiz/App/app_gfx.c"
#include "app_ui.h"
#include "app_link.h"

uint8_t g_sdram_ok;
static wave_window_t sample;
static uint16_t *visible;
static unsigned commands;
uint8_t drv_lcd_init(uint16_t *p) { visible=p; return 0; }
void drv_lcd_set_fb(uint16_t *p) { visible=p; }
void drv_lcd_set_fb_nowait(uint16_t *p) { visible=p; }
void drv_lcd_wait_vsync(void) {}
uint8_t app_link_get_window(wave_window_t *w) { *w=sample; return 1; }
void app_link_cmd_scan(uint8_t v) { commands++; }
void app_link_cmd_run(uint8_t v) { commands++; }
void app_link_cmd_sync(void) { commands++; }
void app_link_cmd_arm(void) { commands++; }
void app_link_cmd_sqw(uint8_t a, uint32_t b) { commands++; }
void app_link_cmd_trig(uint8_t a,uint8_t b,uint8_t c,uint16_t d) { commands++; }

static void save(const char *name)
{
    char path[100];
    sprintf(path,"%s.ppm",name);
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n480 272\n255\n");
    for(int i=0;i<480*272;i++) {
        uint16_t c=visible[i];
        unsigned char rgb[]={ (unsigned char)(((c>>11)&31)*255/31),
                              (unsigned char)(((c>>5)&63)*255/63),
                              (unsigned char)((c&31)*255/31) };
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}

static void draw(void) { ui_draw(&sample); }
static void click(int x,int y)
{
    ui_touch_set(x,y,1);
    int id=ui_toolbar_hit(x,y);
    if(id>=0) app_ui_widget(id);
    draw();
    ui_touch_set(-1,-1,0); draw();
    /* Some menu actions invalidate their page after drawing the old values. */
    if(app_ui_needs_refresh()) draw();
}

static unsigned count_color(uint16_t color,int top,int bottom)
{
    unsigned n=0;
    for(int y=top;y<bottom;y++) for(int x=0;x<480;x++) n+=visible[y*480+x]==color;
    return n;
}

int main(int argc,char **argv)
{
    const uint32_t us[]=SCOPE_SAMPLE_US,ns[]=SCOPE_SAMPLE_NS,arr[]=SCOPE_TIM2_ARR;
    for(int t=0;t<5;t++) {
        assert(arr[t]+1==us[t]&&ns[t]==us[t]*1000);
        assert(ui_scan_div_ns(t)==ns[t]*PPD);
    }
    int triple=argc>1;
    s_triple=(uint8_t)triple;
    for(int i=0;i<3;i++) { s_fbs[i]=calloc(FB_PIXELS,2); assert(s_fbs[i]); }
    s_fb_idx=1; s_fb=s_fbs[1]; s_show=s_fbs[0]; visible=s_fb;
    app_ui_init(); bg_tables_init(); bg_at_update_trig();
    /* Restoring an old trace must produce exactly the static grid/trigger. */
    for(int src=0;src<5;src++) {
        g_ui.trig_src=(uint8_t)src;
        bg_at_update_trig(); draw_grid(); draw_trig();
        for(int y=WV_Y;y<WV_BOT;y++) for(int x=0;x<480;x++)
            assert(s_fb[y*480+x]==bg_at(x,y));
    }
    g_ui.trig_src=TRIGSRC_CH2;
    sample.n=NPT; sample.dt_ns=20000; sample.scan_code=2;
    for(int i=0;i<NPT;i++) {
        sample.ch1[i]=900+(i%60)*20;
        sample.ch2[i]=(i%50<25)?3800:160;
        sample.dig[i]=((i/30)&1)|(((i/60)&1)<<1)|(((i/25)&1)<<2);
    }
    draw();
    assert(count_color(C_CH1,ANA_Y,ANA_BOT)>100);
    assert(count_color(C_CH2,ANA_Y,ANA_BOT)>100);
    assert(count_color(C_DIG0,DIG_Y,DIG_Y+DIG_H)>100);
    assert(count_color(C_DIG1,DIG_Y+DIG_H,DIG_Y+DIG_H*2)>100);
    assert(count_color(C_DIG2,DIG_Y+DIG_H*2,WV_BOT)>100);
    assert(ana_y(0)==ANA_BOT-1 && ana_y(4095)==ANA_Y);
    assert(x_of(499,&sample)==479);
    save(triple?"mixed-triple":"mixed-single");
    /* Independent channel switches and all eight touch targets. */
    for(int i=0;i<8;i++) assert(ui_toolbar_hit(i*60+30,256)==i);
    click(150,256); assert(!(g_ui.ch_mask&CHM_CH1));
    assert(count_color(C_CH1,ANA_Y,ANA_BOT)==0);
    assert(count_color(C_CH2,ANA_Y,ANA_BOT)>100);
    click(150,256); assert(g_ui.ch_mask==31);
    const uint16_t colors[5]={C_CH1,C_CH2,C_DIG0,C_DIG1,C_DIG2};
    for(int i=0;i<5;i++) {
        int top=i<2?ANA_Y:DIG_Y+(i-2)*DIG_H;
        int bottom=i<2?ANA_BOT:top+DIG_H;
        click((i+2)*60+30,256);
        assert(!(g_ui.ch_mask&(1u<<i)));
        assert(count_color(colors[i],top,bottom)==0);
        click((i+2)*60+30,256);
        assert(g_ui.ch_mask==31);
    }
    /* Open/operate/close repeatedly, in both rendering paths. */
    for(int repeat=0;repeat<3;repeat++) {
        unsigned before=commands;
        click(450,256); assert(ui_page_get()==UI_PAGE_SETTINGS);
        assert(commands==before+1); /* Opening MENU must not also hit SYNC. */
        click(80,44); /* Trigger tab. */
        uint8_t src=g_ui.trig_src; click(300,79); assert(g_ui.trig_src==(src+1)%5);
        click(300,117); click(300,155);
        click(200,193); click(430,193);
        click(120,244); click(350,244);
        save(triple?"trigger-triple":"trigger-single");
        assert(visible[2*480+386]==C_EDGE); /* BACK survives subsequent redraws. */
        click(220,44); assert(g_ui.tab==TAB_SQW);
        uint32_t frequency=g_ui.sqw_freq;
        click(430,148); assert(g_ui.sqw_freq!=frequency);
        click(340,92);
        save(triple?"output-triple":"output-single");
        click(390,44); assert(g_ui.tab==TAB_MEAS);
        click(100,244);
        save(triple?"measure-triple":"measure-single");
        click(426,12); assert(ui_page_get()==UI_PAGE_WAVE);
        draw();
        assert(count_color(C_DIG2,DIG_Y+DIG_H*2,WV_BOT)>100);
    }
    /* All timebases use the same 500-point geometry; paused redraw keeps it. */
    for(int t=0;t<5;t++) {
        sample.scan_code=t; sample.dt_ns=ui_scan_div_ns(t)/PPD;
        draw(); assert(x_of(499,&sample)==479);
    }
    /* Automatic measurements use the frame timing, not the requested timebase. */
    ui_meas_all_t measured;
    sample.scan_code=2; sample.dt_ns=20000;
    for(int i=0;i<NPT;i++) {
        sample.ch1[i]=2048;
        sample.ch2[i]=(i%50<25)?4095:0;
        sample.dig[i]=(i%50<25)?7:0;
    }
    g_ui.scan=4;
    ui_measure(&sample,&measured);
    assert(measured.ch1.vpp==0 && !measured.ch1.valid);
    assert(fabsf(measured.ch1.vavg-ui_code_to_volt(2048))<0.00001f);
    assert(fabsf(measured.ch2.freq-1000)<0.01f);
    assert(fabsf(measured.ch2.vpp-3.3f)<0.00001f);
    assert(fabsf(measured.dig2.freq-1000)<0.01f);
    assert(measured.dig2.duty==0.5f);
    assert(fabsf(measured.dig2.width-0.0005f)<0.000001f);
    for(int bit=0;bit<3;bit++) {
        for(int i=0;i<NPT;i++) sample.dig[i]=(uint8_t)(1u<<bit);
        ui_measure(&sample,&measured);
        ui_meas_t *value=bit==0?&measured.dig0:bit==1?&measured.dig1:&measured.dig2;
        assert(value->duty==1 && value->freq==0);
    }
    /* Starting high and observing just one rise does not contain a full period. */
    for(int i=0;i<NPT;i++) sample.dig[i]=(i<100 || i>=300)?7:0;
    ui_measure(&sample,&measured);
    assert(measured.dig0.freq==0 && measured.dig1.freq==0 && measured.dig2.freq==0);
    sample.dt_ns=0;
    ui_measure(&sample,&measured);
    assert(measured.ch2.freq==0 && measured.dig2.freq==0);
    sample.n=1; ui_measure(&sample,&measured);
    assert(!measured.ch1.valid && !measured.dig0.valid);
    sample.n=NPT; sample.dt_ns=20000;

    /* Menu has exactly three full-width tabs. */
    click(450,256);
    click(80,44); assert(g_ui.tab==TAB_TRIG);
    click(240,44); assert(g_ui.tab==TAB_SQW);
    click(420,44); assert(g_ui.tab==TAB_MEAS);
    click(426,12); assert(ui_page_get()==UI_PAGE_WAVE);
    draw();
    click(30,256); assert(!g_ui.run);
    unsigned before=commands;
    uint16_t held[WAVE_W*WV_H];
    memcpy(held,visible+WV_Y*SCR_W,sizeof(held));
    click(200,100); // Waveform touches have no measurement action.
    assert(!g_ui.run && commands==before);
    assert(memcmp(held,visible+WV_Y*SCR_W,sizeof(held))==0);
    save(triple?"hold-triple":"hold-single");
    puts("PASS: automatic voltage/frequency/duty measurements, partial-period rejection and HOLD");
    assert(commands>10);
    puts(triple?"PASS: triple-buffer mixed display, menus and touch actions":"PASS: single-buffer mixed display, menus and touch actions");
    return 0;
}
