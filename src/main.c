#include <pebble.h>
#include "main.h"
static Window *s_main_window;
static TextLayer *s_battery_layer;
static TextLayer *s_mail_count_layer;
static TextLayer *s_sms_count_layer;
static TextLayer *s_phone_count_layer;
static TextLayer *s_phone_battery_layer;
static BitmapLayer *s_connection_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static BitmapLayer *s_bar_layer;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;
static GBitmap *s_bar_bitmap;
static GBitmap *s_active_connection_bitmap;
static GBitmap *s_active_connection_bitmap0;
static GBitmap *s_active_connection_bitmap1;
static GBitmap *s_active_connection_bitmap2;
static GBitmap *s_active_connection_bitmap3;
static GBitmap *s_active_connection_bitmap4;
static GBitmap *s_active_connection_bitmap5;
static GBitmap *s_active_connection_bitmap6;
static GBitmap *s_active_connection_bitmap7;
static GBitmap *s_active_connection_bitmap8;
static GBitmap *s_inactive_connection_bitmap;
static int current_theme;
static int last_phone_battery_value;
static int coinflip;

// imported method to manage smartwatch+ stuff
static uint32_t s_sequence_number = 0xFFFFFFFE;
static char sms_count_str[5], mail_count_str[5], phone_count_str[5];
static int phoneBatteryPercent;

AppMessageResult sm_message_out_get(DictionaryIterator **iter_out) {
    AppMessageResult result = app_message_outbox_begin(iter_out);
    if(result != APP_MSG_OK) return result;
    dict_write_int32(*iter_out, SM_SEQUENCE_NUMBER_KEY, ++s_sequence_number);
    if(s_sequence_number == 0xFFFFFFFF) {
        s_sequence_number = 1;
    }
    return APP_MSG_OK;
}

void reset_sequence_number() {
    DictionaryIterator *iter = NULL;
    app_message_outbox_begin(&iter);
    if(!iter) return;
    dict_write_int32(iter, SM_SEQUENCE_NUMBER_KEY, 0xFFFFFFFF);
    app_message_outbox_send();
}


void sendCommand(int key) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, -1);
	app_message_outbox_send();
}


void sendCommandInt(int key, int param) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, param);
	app_message_outbox_send();
}

// handlers
void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback
  static char phone_battery_text[] = "100%";  
	Tuple *t;
  t=dict_find(received, SM_COUNT_MAIL_KEY);
  if (t!=NULL) {
    memcpy(mail_count_str, t->value->cstring, strlen(t->value->cstring));
    mail_count_str[strlen(t->value->cstring)] = '\0';
    text_layer_set_text(s_mail_count_layer, mail_count_str);
  }
	t=dict_find(received, SM_COUNT_SMS_KEY);
  if (t!=NULL) {
    memcpy(sms_count_str, t->value->cstring, strlen(t->value->cstring));
    sms_count_str[strlen(t->value->cstring)] = '\0';
    text_layer_set_text(s_sms_count_layer, sms_count_str);
  }
	t=dict_find(received, SM_COUNT_PHONE_KEY); 
  if (t!=NULL) {
    memcpy(phone_count_str, t->value->cstring, strlen(t->value->cstring));
    phone_count_str[strlen(t->value->cstring)] = '\0';
    text_layer_set_text(s_phone_count_layer, phone_count_str);
  }
	t=dict_find(received, SM_COUNT_BATTERY_KEY); 
	if (t!=NULL) {
    phoneBatteryPercent = t->value->uint8;
    // alert for fully charged or low phone battery
    if ((phoneBatteryPercent == 100 && last_phone_battery_value == 99) || (phoneBatteryPercent == 20 && last_phone_battery_value == 21)) {
      vibes_double_pulse();
    }
    else if ((phoneBatteryPercent == 80 && last_phone_battery_value == 79) || (phoneBatteryPercent == 40 && last_phone_battery_value == 41)) {
      vibes_short_pulse();
    }
    last_phone_battery_value = t->value->uint8;
    snprintf(phone_battery_text, sizeof(phone_battery_text), "%d", phoneBatteryPercent);
    text_layer_set_text(s_phone_battery_layer, phone_battery_text);
	}
}

// Battery handler
static void handle_battery(BatteryChargeState charge_state) {  
  static char battery_text[] = "100%";
  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "%d%%*", charge_state.charge_percent);
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(s_battery_layer, battery_text);
}

// Connection handler
static void handle_bluetooth(bool connected) {  
  if (connected) {
    bitmap_layer_set_bitmap(s_connection_layer, s_active_connection_bitmap);  
  }
  else {
    bitmap_layer_set_bitmap(s_connection_layer, s_inactive_connection_bitmap);  
  }
  vibes_short_pulse();
}

// Time updater
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  // Create a long-lived buffer
  static char buffer[] = "00:00";  
  // Write the current hours and minutes into the buffer   
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }  
  // Display time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);  
}

// Change theme according to current theme value
static void change_theme(bool forcechange) {
  // Get background and active connection icon acccording to day or according to current theme settings
  coinflip = rand()%2;
  if (!forcechange) {
    // Get a tm structure
    time_t temp = time(NULL); 
    struct tm *tick_time = localtime(&temp);  
    // Create a long-lived buffer
    char buffer3[] = "1";
    strftime(buffer3, sizeof(buffer3), "%w", tick_time);
    current_theme = atoi(buffer3);
  }
  if (current_theme == 0) {
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_SUNDAY);
    }
    else {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_SUNDAY2);
    }
    s_active_connection_bitmap = s_active_connection_bitmap0;
  }
  else if (current_theme == 1) {
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_MONDAY);
    }
    else {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_MONDAY2);
    }
    s_active_connection_bitmap = s_active_connection_bitmap1;
  }
  else if (current_theme == 2) {
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_TUESDAY);
    }
    else {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_TUESDAY2);
    }
    s_active_connection_bitmap = s_active_connection_bitmap2;
  }
  else if (current_theme == 3) {
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_WEDNESDAY);
    }
    else {
      coinflip = rand()%2;
      if (coinflip == 0) {
        s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_WEDNESDAY2);
      }
      else {
        s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_WEDNESDAY3);
      }
    }
    s_active_connection_bitmap = s_active_connection_bitmap3;
  }
  else if (current_theme == 4) {
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_THURSDAY);
    }
    else {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_THURSDAY2);
    }
    s_active_connection_bitmap = s_active_connection_bitmap4;
  }
  else if (current_theme == 5) {
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_FRIDAY);
    }
    else {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_FRIDAY2);
    }
    s_active_connection_bitmap = s_active_connection_bitmap5;
  }
  else if (current_theme == 6) {    
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_SATURDAY);
    }
    else {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_SATURDAY2);
    }
    s_active_connection_bitmap = s_active_connection_bitmap6;
  }
  else if (current_theme == 7) {    
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_CHIBI);
    }
    else {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_CHIBI2);
    }
    s_active_connection_bitmap = s_active_connection_bitmap7;
  }
  else if (current_theme == 8) {    
    gbitmap_destroy(s_background_bitmap);
    if (coinflip == 0) {
      s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_URANEP);
    }
    else {
      coinflip = rand()%2;
      if (coinflip == 0) {
        s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_URANEP2);
      }
      else {
        s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_URANEP3);
      }
    }
    s_active_connection_bitmap = s_active_connection_bitmap8;
  }
  if (bluetooth_connection_service_peek()) {
    bitmap_layer_set_bitmap(s_connection_layer, s_active_connection_bitmap);
    bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  }
  else {
    bitmap_layer_set_bitmap(s_connection_layer, s_inactive_connection_bitmap);
    bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  }  
}

// Date and active connection icon updater
static void update_date() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);  
  // Create a long-lived buffer
  static char buffer2[] = "Mon Feb 29";
  // Write the current date into the buffer
  strftime(buffer2, sizeof("Mon Feb 29"), "%a %b %e", tick_time);
  // Display date on the TextLayer
  text_layer_set_text(s_date_layer, buffer2);
  // Change theme
  change_theme(false);  
}

// Tap handler to force theme change
static void tap_handler(AccelAxisType axis, int32_t direction) {
  switch (axis) {
  case ACCEL_AXIS_X:
  case ACCEL_AXIS_Y:
    break;
  case ACCEL_AXIS_Z:
    if (direction > 0) {
      current_theme++;
    } else {
      current_theme--;
    }
    break;
  }
  if (current_theme > 8) {
    current_theme = 0;  
  }
  else if (current_theme < 0) {
    current_theme = 8;  
  }
  change_theme(true);
}

// Tick handler with multiplexer MINUTE_UNIT - DAY_UNIT
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
   if (units_changed & MINUTE_UNIT) {
    update_time();  
  }
  if (units_changed & DAY_UNIT) {
    update_date();
  }  
}

static void main_window_appear(Window *window)
{
  sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);	
}

static void main_window_disappear(Window *window)
{
	sendCommandInt(SM_SCREEN_EXIT_KEY, STATUS_SCREEN_APP);	
}

void reconnect(void *data) {
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);	
}


// Main window loader
static void main_window_load(Window *window) {
  // Create GBitmap, then set to created BitmapLayer
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);  

  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(0, 10, 144, 44));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00");
  
  // Create date TextLayer
  s_date_layer = text_layer_create(GRect(0, 0, 144, 19));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_text(s_date_layer, "Mon Jan 1");
  
  // Create battery TextLayer
  s_battery_layer = text_layer_create(GRect(0, 0, 144, 18));
  text_layer_set_text_color(s_battery_layer, GColorWhite);
  text_layer_set_background_color(s_battery_layer, GColorClear);
  text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);
  text_layer_set_text(s_battery_layer, "-");
  handle_battery(battery_state_service_peek());
  
  // Create mail count TextLayer
  s_mail_count_layer = text_layer_create(GRect(13, 154, 20, 15));
  text_layer_set_text_color(s_mail_count_layer, GColorWhite);
  text_layer_set_background_color(s_mail_count_layer, GColorClear);
  text_layer_set_font(s_mail_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_mail_count_layer, GTextAlignmentLeft);
  text_layer_set_text(s_mail_count_layer, "-");
  
  // Create sms count TextLayer
  s_sms_count_layer = text_layer_create(GRect(50, 154, 20, 15));
  text_layer_set_text_color(s_sms_count_layer, GColorWhite);
  text_layer_set_background_color(s_sms_count_layer, GColorClear);
  text_layer_set_font(s_sms_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_sms_count_layer, GTextAlignmentLeft);
  text_layer_set_text(s_sms_count_layer, "-");
  
  // Create phone count TextLayer
  s_phone_count_layer = text_layer_create(GRect(87, 154, 20, 15));
  text_layer_set_text_color(s_phone_count_layer, GColorWhite);
  text_layer_set_background_color(s_phone_count_layer, GColorClear);
  text_layer_set_font(s_phone_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_phone_count_layer, GTextAlignmentLeft);
  text_layer_set_text(s_phone_count_layer, "-");
  
  // Create phone battery TextLayer
  s_phone_battery_layer = text_layer_create(GRect(124, 154, 20, 15));
  text_layer_set_text_color(s_phone_battery_layer, GColorWhite);
  text_layer_set_background_color(s_phone_battery_layer, GColorClear);
  text_layer_set_font(s_phone_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_phone_battery_layer, GTextAlignmentLeft);
  text_layer_set_text(s_phone_battery_layer, "-");
  
  // Create connection BitmapLayer
  s_active_connection_bitmap = s_active_connection_bitmap1;
  s_connection_layer = bitmap_layer_create(GRect(0, 1, 19, 19));
  bitmap_layer_set_bitmap(s_connection_layer, s_active_connection_bitmap);  
  
  // Create bar BitmapLayer
  s_bar_layer = bitmap_layer_create(GRect(0, 158, 144, 10));
  bitmap_layer_set_bitmap(s_bar_layer, s_bar_bitmap);  

  // Improving layout
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add child layers to the Window's root layer
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_battery_layer));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_connection_layer));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_bar_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_mail_count_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_sms_count_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_phone_count_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_phone_battery_layer));
}
static void main_window_unload(Window *window) {
  // Destroy GBitmap
  gbitmap_destroy(s_active_connection_bitmap8);
  gbitmap_destroy(s_active_connection_bitmap7);
  gbitmap_destroy(s_active_connection_bitmap6);
  gbitmap_destroy(s_active_connection_bitmap5);
  gbitmap_destroy(s_active_connection_bitmap4);
  gbitmap_destroy(s_active_connection_bitmap3);
  gbitmap_destroy(s_active_connection_bitmap2);
  gbitmap_destroy(s_active_connection_bitmap1);
  gbitmap_destroy(s_active_connection_bitmap0);
  gbitmap_destroy(s_inactive_connection_bitmap);
  gbitmap_destroy(s_bar_bitmap);
  gbitmap_destroy(s_background_bitmap);
  // Unsubscribe to TickTimerService, Accel, Battery and Connection services
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
}

// Init
static void init() {
  last_phone_battery_value = 0;
  // Loading assets
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_MONDAY);
  s_bar_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SM_BAR);
  s_active_connection_bitmap0 = gbitmap_create_with_resource(RESOURCE_ID_B_SUNDAY);
  s_active_connection_bitmap1 = gbitmap_create_with_resource(RESOURCE_ID_B_MONDAY);
  s_active_connection_bitmap2 = gbitmap_create_with_resource(RESOURCE_ID_B_TUESDAY);
  s_active_connection_bitmap3 = gbitmap_create_with_resource(RESOURCE_ID_B_WEDNESDAY);
  s_active_connection_bitmap4 = gbitmap_create_with_resource(RESOURCE_ID_B_THURSDAY);
  s_active_connection_bitmap5 = gbitmap_create_with_resource(RESOURCE_ID_B_FRIDAY);
  s_active_connection_bitmap6 = gbitmap_create_with_resource(RESOURCE_ID_B_SATURDAY);
  s_active_connection_bitmap7 = gbitmap_create_with_resource(RESOURCE_ID_B_CHIBI);
  s_active_connection_bitmap8 = gbitmap_create_with_resource(RESOURCE_ID_B_URANEP);  
  s_inactive_connection_bitmap = gbitmap_create_with_resource(RESOURCE_ID_B_NONE);  
  // Create main Window element and assign to pointer
  s_main_window = window_create();  
  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
    .appear = main_window_appear,
    .disappear = main_window_disappear});  
  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);  
  // Register with TickTimerService, Accel, Battery and Connection services
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_tap_service_subscribe(tap_handler);
  battery_state_service_subscribe(handle_battery);
  bluetooth_connection_service_subscribe(handle_bluetooth);  
  // Make sure the time, date and icon are displayed from the start
  update_time();
  update_date();
}

// Deinit
static void deinit() {
  // Destroy BitmapLayer
  bitmap_layer_destroy(s_connection_layer);
  bitmap_layer_destroy(s_bar_layer);  
  bitmap_layer_destroy(s_background_layer);
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_phone_battery_layer);
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_date_layer); 
  // Destroy Window
  window_destroy(s_main_window);    
}

static void appmsg_in_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i", reason);
}

// Main
int main(void) {
  app_message_register_inbox_received(rcv);
  app_message_register_inbox_dropped(appmsg_in_dropped);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  init();
  app_event_loop();
  app_message_deregister_callbacks();
  deinit();
}