#ifndef MADO
#define MADO

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
struct mado {
    const char *title;
    const int width, height;
    uint32_t *buf;
    int keys[256]; /* keys are mostly ASCII, but arrows are 17..20 */
    int mod;       /* mod is 4 bits mask, ctrl=1, shift=2, alt=4, meta=8 */
    int x, y;
    int mouse;
    void *event_queue;
    uint32_t event_count;
    size_t event_queue_start;
};

#ifndef MADO_API
#define MADO_API extern
#endif
MADO_API int mado_open(struct mado *m);
MADO_API int mado_loop(struct mado *m);
MADO_API void mado_close(struct mado *m);
MADO_API void mado_sleep(int64_t ms);
MADO_API int64_t mado_time(void);
#define mado_pixel(m, x, y) ((m)->buf[((y) * (m)->width) + (x)])

#ifndef MADO_HEADER
#define RV_QUEUE_CAPACITY 128

enum {
    RV_KEYCODE_RETURN = 0x0000000D,
    RV_KEYCODE_UP = 0x40000052,
    RV_KEYCODE_DOWN = 0x40000051,
    RV_KEYCODE_RIGHT = 0x4000004F,
    RV_KEYCODE_LEFT = 0x40000050,
    RV_KEYCODE_LCTRL = 0x400000E0,
    RV_KEYCODE_RCTRL = 0x400000E4,
    RV_KEYCODE_LSHIFT = 0x400000E1,
    RV_KEYCODE_RSHIFT = 0x400000E5,
    RV_KEYCODE_LALT = 0x400000E2,
    RV_KEYCODE_RALT = 0x400000E6,
    RV_KEYCODE_LMETA = 0x400000E3,
    RV_KEYCODE_RMETA = 0x400000E7,
};
//KEY_EVENT
typedef struct {
    uint32_t keycode;
    uint8_t state;
    uint16_t mod;
} rv_key_rv_event_t;
//MOUSE_MOTION_EVENT
typedef struct {
    int32_t x, y, xrel, yrel;
} rv_mouse_motion_t;

enum {
    RV_MOUSE_BUTTON_LEFT = 1,
};
//MOUSE_BUTTON_EVENT
typedef struct {
    uint8_t button;
    uint8_t state;
} rv_mouse_button_t;

enum {
    RV_KEY_EVENT = 0,
    RV_MOUSE_MOTION_EVENT = 1,
    RV_MOUSE_BUTTON_EVENT = 2,
    RV_QUIT_EVENT = 3,
};

typedef struct {
    uint32_t type;
    union {
        rv_key_rv_event_t key_event;
        union {
            rv_mouse_motion_t motion;
            rv_mouse_button_t button;
        } mouse;
    };
} rv_event_t;

enum {
    RV_RELATIVE_MODE_SUBMISSION = 0,
    RV_WINDOW_TITLE_SUBMISSION = 1,
};

typedef struct {
    uint8_t enabled;
} rv_mouse_rv_submission_t;

typedef struct {
    uint32_t title;
    uint32_t size;
} rv_title_rv_submission_t;

typedef struct {
    uint32_t type;
    union {
        rv_mouse_rv_submission_t mouse;
        rv_title_rv_submission_t title;
    };
} rv_submission_t;

MADO_API int mado_open(struct mado *m)
{
    m->event_queue = malloc((sizeof(rv_event_t) + sizeof(rv_submission_t)) *
                            RV_QUEUE_CAPACITY);
    m->event_count = 0;
    register int a0 __asm("a0") = (int) m->event_queue;
    register int a1 __asm("a1") = RV_QUEUE_CAPACITY;
    register int a2 __asm("a2") = (int) &m->event_count;
    register int a7 __asm("a7") = 0xc0de;
    __asm volatile("scall" : : "r"(a0), "r"(a1), "r"(a2), "r"(a7) : "memory");
    m->event_queue_start = 0;

    rv_submission_t submission = {
        .type = RV_WINDOW_TITLE_SUBMISSION,
        .title.title = (uint32_t) m->title,
        .title.size = strlen(m->title),
    };
    rv_submission_t *submission_queue =
        (rv_submission_t *) ((rv_event_t *) m->event_queue + RV_QUEUE_CAPACITY);
    submission_queue[0] = submission;
    a0 = 1;
    a7 = 0xfeed;
    __asm volatile("scall" : : "r"(a0), "r"(a7) : "memory");

    return 0;
}

MADO_API void mado_close(struct mado *m)
{
    free(m->event_queue);
}

MADO_API int mado_loop(struct mado *m)
{
    for (; m->event_count > 0; m->event_count--) {
        rv_event_t event =
            ((rv_event_t *) m->event_queue)[m->event_queue_start];
        switch (event.type) {
        case RV_KEY_EVENT: {
            uint32_t keycode = event.key_event.keycode;
            uint8_t state = event.key_event.state;

            if (keycode == RV_KEYCODE_RETURN)
                m->keys[10] = state;
            else if (keycode < 128)
                m->keys[keycode] = state;

            if (keycode == RV_KEYCODE_UP)
                m->keys[17] = state;
            if (keycode == RV_KEYCODE_DOWN)
                m->keys[18] = state;
            if (keycode == RV_KEYCODE_RIGHT)
                m->keys[19] = state;
            if (keycode == RV_KEYCODE_LEFT)
                m->keys[20] = state;

            if (keycode == RV_KEYCODE_LCTRL || keycode == RV_KEYCODE_RCTRL)
                m->mod = state ? m->mod | 1 : m->mod & 0b1110;
            if (keycode == RV_KEYCODE_LSHIFT || keycode == RV_KEYCODE_RSHIFT)
                m->mod = state ? m->mod | 2 : m->mod & 0b1101;
            if (keycode == RV_KEYCODE_LALT || keycode == RV_KEYCODE_RALT)
                m->mod = state ? m->mod | 4 : m->mod & 0b1011;
            if (keycode == RV_KEYCODE_LMETA || keycode == RV_KEYCODE_RMETA)
                m->mod = state ? m->mod | 8 : m->mod & 0b0111;

            break;
        }
        case RV_MOUSE_MOTION_EVENT:
            m->x = event.mouse.motion.x;
            m->y = event.mouse.motion.y;
            break;
        case RV_MOUSE_BUTTON_EVENT:
            if (event.mouse.button.button == RV_MOUSE_BUTTON_LEFT)
                m->mouse = event.mouse.button.state;
            break;
        case RV_QUIT_EVENT:
            return -1;
        }
        m->event_queue_start =
            (m->event_queue_start + 1) & (RV_QUEUE_CAPACITY - 1);
    }

    register int a0 __asm("a0") = (uintptr_t) m->buf;
    register int a1 __asm("a1") = m->width;
    register int a2 __asm("a2") = m->height;
    register int a7 __asm("a7") = 0xbeef;
    __asm volatile("scall" : : "r"(a0), "r"(a1), "r"(a2), "r"(a7) : "memory");
    return 0;
}

#undef RV_QUEUE_CAPACITY

MADO_API int64_t mado_time(void)
{
    return (int64_t) clock() / (CLOCKS_PER_SEC / 1000.0f);
}
MADO_API void mado_sleep(int64_t ms)
{
    int64_t start = mado_time();
    while (mado_time() - start < ms)
        ;
}

#ifdef __cplusplus
class Mado
{
    struct mado m;
    int64_t now;

public:
    Mado(const int w, const int h, const char *title)
        : m{.title = title, .width = w, .height = h}
    {
        this->m.buf = new uint32_t[w * h];
        this->now = mado_time();
        mado_open(&this->m);
    }
    ~Mado()
    {
        mado_close(&this->m);
        delete[] this->m.buf;
    }
    bool loop(const int fps)
    {
        int64_t t = mado_time();
        if (t - this->now < 1000 / fps) {
            mado_sleep(t - now);
        }
        this->now = t;
        return mado_loop(&this->m) == 0;
    }
    inline uint32_t &px(const int x, const int y)
    {
        return mado_pixel(&this->m, x, y);
    }
    bool key(int c) { return c >= 0 && c < 128 ? this->m.keys[c] : false; }
    int x() { return this->m.x; }
    int y() { return this->m.y; }
    int mouse() { return this->m.mouse; }
    int mod() { return this->m.mod; }
};
#endif /* __cplusplus */

#endif /* !MADO_HEADER */
#endif /* MADO_H */
