#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "LCD_0in96.h"
#include "ImageData.h"

#include "lvgl.h"

static lv_disp_draw_buf_t display_buf;
static lv_color_t buffer1[LCD_0IN96_HEIGHT * LCD_0IN96_WIDTH];

void lvgl_flush_cb(lv_disp_drv_t *disp_drv,
                   const lv_area_t *area,
                   lv_color_t *color_p) {

    LCD_0IN96_DisplayWindows(
            area->x1,
            area->y1,
            area->x2,
            area->y2,
            (UWORD *) color_p
    );

    // LCD_0IN96_Display((UWORD*)color_p);

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}

static uint64_t tick = 0;

bool repeating_timer_callback(struct repeating_timer *t) {

    lv_timer_handler();

    uint64_t inc = (time_us_64() - tick);
    lv_tick_inc(inc / 1000);
    tick += inc;

    return true;
}

static void btn_event_cb(lv_event_t *e) {

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        int cnt = (int) btn->user_data;
        cnt++;
        btn->user_data = (void *) cnt;

        /*Get the first child of the button which is the label and change
         * its text*/
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Button: %d", cnt);
    }
}

/**
 * Create a playback animation
 */
void example_ui(void) {

    lv_obj_t *btn =
            lv_btn_create(lv_scr_act()); /*Add a button the current screen*/
    btn->user_data = (void *) 0;
    lv_obj_set_pos(btn, 5, 5);    /*Set its position*/
    lv_obj_set_size(btn, 60, 20); /*Set its size*/
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL,
                        NULL); /*Assign a callback to the button*/

    lv_obj_t *label = lv_label_create(btn); /*Add a label to the button*/
    lv_label_set_text(label, "Button");     /*Set the labels text*/
    lv_obj_center(label);

    lv_obj_t *btn2 =
            lv_btn_create(lv_scr_act()); /*Add a button the current screen*/
    btn->user_data = (void *) 0;
    lv_obj_set_pos(btn2, 5, 30);   /*Set its position*/
    lv_obj_set_size(btn2, 60, 20); /*Set its size*/
    lv_obj_add_event_cb(btn2, btn_event_cb, LV_EVENT_ALL,
                        NULL); /*Assign a callback to the button*/

    lv_obj_t *label2 = lv_label_create(btn2); /*Add a label to the button*/
    lv_label_set_text(label2, "Button");      /*Set the labels text*/
    lv_obj_center(label2);

    lv_group_add_obj(lv_group_get_default(), btn);
    lv_group_add_obj(lv_group_get_default(), btn2);
}

#define SUPPORTED_KEYS 7

const int KEY_UP = 2;
const int KEY_DOWN = 18;
const int KEY_LEFT = 16;
const int KEY_RIGHT = 20;
const int KEY_CTRL = 3;
const int KEY_A = 15;
const int KEY_B = 17;

static int key_map[SUPPORTED_KEYS][2] = {
        {KEY_UP,    LV_KEY_UP},
        {KEY_DOWN,  LV_KEY_DOWN},
        {KEY_LEFT,  LV_KEY_LEFT},
        {KEY_RIGHT, LV_KEY_RIGHT},
        {KEY_A,     LV_KEY_ENTER},
        {KEY_B,     LV_KEY_ESC},
        {KEY_CTRL,  LV_KEY_HOME}};

static volatile int current_pressed = 0;
static volatile int last_pressed = LV_KEY_UP;

int last_key() {
    int some_pressed = 0;
    for (int i = 0; i < SUPPORTED_KEYS; i += 1) {
        if (DEV_Digital_Read(key_map[i][0]) == 0) {
            some_pressed = 1;
            last_pressed = key_map[i][1];
            break;
        }
    }

    current_pressed = some_pressed;

    return last_pressed;
}

int key_pressed() {
    return current_pressed;
}

void keyboard_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {

    data->key = last_key(); /*Get the last pressed or released key*/

    if (key_pressed()) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void encoder_with_keys_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {

    const int key = last_key();
    const int left_or_right = (key == LV_KEY_LEFT || key == LV_KEY_RIGHT);

    if (left_or_right) {
        data->key = key;
    }
    if (key_pressed() && left_or_right) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void image_copy(const uint8_t *src, uint8_t *dst, int w, int h) {

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            UWORD c = (*(src + j * w * 2 + i * 2 + 1)) << 8 |
                      (*(src + j * w * 2 + i * 2));
            UDOUBLE addr = i * 2 + j * w * 2;
            dst[addr] = 0xff & (c >> 8);
            dst[addr + 1] = 0xff & c;
        }
    }
}

void display_startup() {
    UWORD *BlackImage;
    UDOUBLE Imagesize = LCD_0IN96_HEIGHT * LCD_0IN96_WIDTH * 2;
    if ((BlackImage = (UWORD *) malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        exit(0);
    }
    image_copy(gImage_0inch96_1, (uint8_t *) BlackImage, LCD_0IN96_WIDTH,
               LCD_0IN96_HEIGHT);
    LCD_0IN96_Display(BlackImage);
    free(BlackImage);

    DEV_Delay_ms(3000);
}

int main(void) {

    //  init lvgl
    lv_init();

    DEV_Delay_ms(100);
    printf("LCD_0in96_test Demo\r\n");
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    /* LCD Init */
    printf("0.96inch LCD demo...\r\n");
    LCD_0IN96_Init(HORIZONTAL);
    DEV_SET_PWM(50);

    LCD_0IN96_Clear(0xffff);

    display_startup();

    DEV_KEY_Config(KEY_UP);
    DEV_KEY_Config(KEY_DOWN);
    DEV_KEY_Config(KEY_LEFT);
    DEV_KEY_Config(KEY_RIGHT);
    DEV_KEY_Config(KEY_CTRL);
    DEV_KEY_Config(KEY_A);
    DEV_KEY_Config(KEY_B);

    //  set buffer
    lv_disp_draw_buf_init(&display_buf, buffer1, NULL,
                          LCD_0IN96_HEIGHT * LCD_0IN96_WIDTH);

    static lv_disp_drv_t
            disp_drv; /*A variable to hold the drivers. Must be static or global.*/
    lv_disp_drv_init(&disp_drv);   /*Basic initialization*/
    disp_drv.draw_buf = &display_buf; /*Set an initialized buffer*/
    disp_drv.flush_cb =
            lvgl_flush_cb; /*Set a flush callback to draw to the display*/
    disp_drv.hor_res =
            LCD_0IN96_WIDTH; /*Set the horizontal resolution in pixels*/
    disp_drv.ver_res =
            LCD_0IN96_HEIGHT; /*Set the vertical resolution in pixels*/
    disp_drv.direct_mode = 1;
    disp_drv.full_refresh = 0;

    lv_disp_drv_register(&disp_drv); /*Register the driver and save the
                                            created display objects*/

    //  keyboard
    static lv_indev_drv_t kbrd_drv;
    lv_indev_drv_init(&kbrd_drv); /*Basic initialization*/
    kbrd_drv.type = LV_INDEV_TYPE_KEYPAD;
    kbrd_drv.read_cb = keyboard_read;

    //  encoder
    static lv_indev_drv_t encoder_drv;
    lv_indev_drv_init(&encoder_drv);
    encoder_drv.type = LV_INDEV_TYPE_ENCODER;
    encoder_drv.read_cb = encoder_with_keys_read;

    lv_indev_t *kbrd_dev = lv_indev_drv_register(&kbrd_drv);
    lv_indev_t *encoder_dev = lv_indev_drv_register(&encoder_drv);

    lv_group_set_default(lv_group_create());
    lv_indev_set_group(kbrd_dev, lv_group_get_default());
    lv_indev_set_group(encoder_dev, lv_group_get_default());

    struct repeating_timer timer;
    add_repeating_timer_ms(5, repeating_timer_callback, NULL, &timer);

    example_ui();

    while (1) {
    }

    return 0;
}
