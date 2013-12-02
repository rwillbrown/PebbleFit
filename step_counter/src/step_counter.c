#include <pebble.h>

static Window *window;
static TextLayer *time_layer;
static TextLayer *text_layer;
static AppTimer *accelTimeout;

static char text_layer_buffer[50];
static char timebuf[50];

static int threshold=0;
static int min=10000;
static int max=-10000;
static int sensitivityConstant = 60;

static int steps=0;
static int STEP_KEY=0;
static bool stepsChanged=false;

static int regulation=0;
static int lastStepIndex = 0;
static int lastStepSet=0;

static void handle_accel(AccelData *accel_data, uint32_t num_samples);

float my_sqrt(int num) {
    float a, p, e = 0.001, b;
    a = num;
    p = a * a;
    int nb = 0;
    while ((p - num >= e) && (nb++ < 40)) {
        b = (a + (num / a)) / 2;
        a = b;
        p = a * a;
    }
    return a;
}

static void printSteps()
{
    snprintf(text_layer_buffer, 50, "Steps\n%d\nPrecision\n%d", steps, sensitivityConstant);
    text_layer_set_text(text_layer, text_layer_buffer);
    stepsChanged=false;
}

static void accel_failed()
{
    accel_data_service_unsubscribe();
    text_layer_set_text(text_layer,"Accelerometer\nstopped!");
    accel_data_service_subscribe(25, handle_accel);
    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    accelTimeout = app_timer_register(1000, accel_failed, NULL);
}


static void handle_accel(AccelData *accel_data, uint32_t num_samples) {
    AccelData *data = &accel_data[0];
    if(!app_timer_reschedule(accelTimeout, 1000))
        accelTimeout = app_timer_register(1000,accel_failed,NULL);

    int deltaAccel=0;
    int sampleNew=0, sampleOld=0;
    min=10000;
    max=-10000;
    
    for(int i=0; i<(int)num_samples; i++)
    {
        data = &accel_data[i];
        int x=(int)data->x, y=(int)data->y, z=(int)data->z;
        data = &accel_data[++i];
        x+=(int)data->x;
        y+=(int)data->y;
        z+=(int)data->z;
        data = &accel_data[++i];
        x+=(int)data->x;
        y+=(int)data->y;
        z+=(int)data->z;
        data = &accel_data[++i];
        x+=(int)data->x;
        y+=(int)data->y;
        z+=(int)data->z;
        data = &accel_data[++i];
        x+=(int)data->x;
        y+=(int)data->y;
        z+=(int)data->z;
        
        x=x/5;
        y=y/5;
        z=z/5;
        
        deltaAccel=my_sqrt((x*x)+(y*y)+(z*z));
        
        sampleOld = sampleNew;
        if(abs(deltaAccel - sampleNew) > sensitivityConstant)
            sampleNew = deltaAccel;
        
        
        if((sampleNew<threshold && sampleOld>threshold) && sampleOld!=0)
        {
            if((lastStepSet<=3) && (i+(50*lastStepSet-lastStepIndex)>10))
            {
                regulation++;
                if (regulation==5)
                {
                    steps+=5;
                    stepsChanged=true;
                }
                else if (regulation>5)
                {
                    steps++;
                    stepsChanged=true;
                }
            }
            else
            {
                regulation=0;
            }
            lastStepSet=0;
            lastStepIndex=i;
        }
        
        if (deltaAccel<min)
            min=deltaAccel;
        if (deltaAccel>max)
            max=deltaAccel;
    }
    
    threshold=(max+min)/2;
    lastStepSet++;
    
    if(stepsChanged)
        printSteps();
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    time_layer = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, 60 } });
    text_layer = text_layer_create((GRect) { .origin = { 0, 60 }, .size = { bounds.size.w, bounds.size.h-60 } });
    text_layer_set_text(text_layer, "Hello World!");
    clock_copy_time_string(timebuf,50);
    text_layer_set_text(time_layer, timebuf);
    text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
    text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
    text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
    text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    layer_add_child(window_layer, text_layer_get_layer(text_layer));
    layer_add_child(window_layer, text_layer_get_layer(time_layer));
}

static void window_unload(Window *window) {
    text_layer_destroy(text_layer);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    steps=0;
    printSteps();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    sensitivityConstant+=10;
    printSteps();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    sensitivityConstant-=10;
    printSteps();
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    clock_copy_time_string(timebuf,50);
    text_layer_set_text(time_layer, timebuf);
}

static void init(void) {
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    
    const bool animated = true;
    window_stack_push(window, animated);
    window_set_click_config_provider(window, click_config_provider);
    accel_data_service_subscribe(25, handle_accel);
    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    accelTimeout = app_timer_register(1000, accel_failed, NULL);
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    
    if (persist_exists(STEP_KEY)) {
        steps = persist_read_int(STEP_KEY);
        printSteps();
    }
}

static void deinit(void) {
    persist_write_int(STEP_KEY, steps);
    window_destroy(window);
}

int main(void) {
    init();
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
    
    app_event_loop();
    deinit();
}
