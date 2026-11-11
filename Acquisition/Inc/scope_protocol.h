/* 两块板共用的协议定义。这里不依赖 HAL，可用于桌面协议测试。 */
#ifndef SCOPE_PROTOCOL_H
#define SCOPE_PROTOCOL_H

#define SCOPE_BAUD             460800u
#define SCOPE_POINTS_PER_DIV   50u
#define SCOPE_DIVISIONS        10u
#define SCOPE_FRAME_POINTS    (SCOPE_POINTS_PER_DIV * SCOPE_DIVISIONS)
#define SCOPE_HEAD0            0xAAu
#define SCOPE_HEAD1            0x55u
#define SCOPE_WAVE_TYPE        0x02u
#define SCOPE_HEADER_BYTES     10u
#define SCOPE_POINT_BYTES      3u
#define SCOPE_FRAME_BYTES     (SCOPE_HEADER_BYTES + SCOPE_FRAME_POINTS * SCOPE_POINT_BYTES + 3u)

#define SCOPE_TIMEBASE_COUNT   5u
#define SCOPE_TIM2_ARR         {199u, 99u, 19u, 9u, 1u}
#define SCOPE_SAMPLE_US        {200u, 100u, 20u, 10u, 2u}
#define SCOPE_SAMPLE_NS        {200000u, 100000u, 20000u, 10000u, 2000u}
#define SCOPE_DIV_NS           {10000000u, 5000000u, 1000000u, 500000u, 100000u}
#define SCOPE_DIV_LABELS       {"10ms", "5ms", "1ms", "500us", "100us"}

/* 保留现有线上编号。DIG2=2，不能按名称顺序重新编号。 */
#define SCOPE_TRIG_CH1         0u
#define SCOPE_TRIG_CH2         1u
#define SCOPE_TRIG_DIG2        2u
#define SCOPE_TRIG_DIG0        3u
#define SCOPE_TRIG_DIG1        4u
#define SCOPE_TRIGGER_SOURCES  5u
#define SCOPE_TRIG_AUTO        0u
#define SCOPE_TRIG_NORMAL      1u
#define SCOPE_TRIG_SINGLE      2u

#endif
