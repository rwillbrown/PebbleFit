#include <pebble.h>

//Variable Declarations
static Window *window;
static TextLayer *time_layer;
static TextLayer *text_layer;
static AppTimer *accelTimeout;

//char[] buffers for output
static char text_layer_buffer[50];
static char timebuf[50];

//step counting variables
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

//SquareRoot calculation
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

//update the UI with new step information
static void printSteps()
{
    snprintf(text_layer_buffer, 50, "Steps\n%d\nPrecision\n%d", steps, sensitivityConstant);
    text_layer_set_text(text_layer, text_layer_buffer);
    stepsChanged=false;
}

/*
 *  If the accelerometer callbacks fail or stop, alert the user. I've attempted
 *  to reregister for the callbacks in this method, but the app crashes.
 */
static void accel_failed()
{
    accel_data_service_unsubscribe();
    text_layer_set_text(text_layer,"Accelerometer\nstopped!");
}

//accelerometer callback
static void handle_accel(AccelData *accel_data, uint32_t num_samples) {
    
    //we got the callback, so reset the timeout timer.
    if(!app_timer_reschedule(accelTimeout, 1000))
        accelTimeout = app_timer_register(1000,accel_failed,NULL);

    int deltaAccel=0;
    int sampleNew=0, sampleOld=0;
    min=10000;
    max=-10000;
    
    for(int i=0; i<(int)num_samples; i++)
    {
        //Get the accel_data structure.
        data = &accel_data[i];
        
        //average the first five samples to filter the input a bit
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
        
        //combine all acceleration vectors into one composite acceleration vector
        deltaAccel=my_sqrt((x*x)+(y*y)+(z*z));
        
        //move sampleNew to SampleOld unconditionally
        sampleOld = sampleNew;
        
        //if the change in acceleration is more than the variable constant, shift the acceleration value into sampleNew
        if(abs(deltaAccel - sampleNew) > sensitivityConstant)
            sampleNew = deltaAccel;
        
        //if the acceleration has moved from above to below the threshold, could be a step
        if((sampleNew<threshold && sampleOld>threshold) && sampleOld!=0)
        {
            /*
             *  Since the samples come in batches of 25, every 1/2 second,
             *  I'm using that information to make sure the steps are in an
             *  acceptable cadence (1/5s to 2s between steps). This translates to
             *  steps being no more than 3 callbacks or "sets" ago, since each
             *  set represents 1/2 second. The steps must also be more than 10
             *  samples apart from each other, fulfilling the no-less-than 1/5
             *  second cadence requirement.
             */
            if((lastStepSet<=3) && (i+(50*lastStepSet-lastStepIndex)>10))
            {
                /*
                 *  There is also a regulation count that forces steps to be
                 *  consecutive before they are counted. There must be 5 steps
                 *  in a row inside the acceptable cadence range for them to be
                 *  counted.
                 */
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
        
        //finding the min and max accel values to calculate the dynamic threshold
        if (deltaAccel<min)
            min=deltaAccel;
        if (deltaAccel>max)
            max=deltaAccel;
    }
    
    //calculating the dynamic threshold
    threshold=(max+min)/2;
    
    //increment the set the last step was found in
    lastStepSet++;
    
    //if we've updated the step count, then redraw the UI
    if(stepsChanged)
        printSteps();
}

//window setup
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

//middle button resets steps
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    steps=0;
    printSteps();
}

//up increases the threshold level
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    sensitivityConstant+=10;
    printSteps();
}

//down decreases the threshold
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    sensitivityConstant-=10;
    printSteps();
}

//setting up button click handlers
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
    
    //subscribe to accelerometer callbacks every 25 samples
    accel_data_service_subscribe(25, handle_accel);
    
    //set sampling rate to 50 hz
    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    
    //set timeout callback in case the accelerometer fails.
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
