#include "pebble.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
	
#define M_PI 3.14
	
// This defines graphics path information to be loaded as a path later
static const GPathInfo LINE_PATH_POINTS = {
  // This is the amount of points
  4,
  // A path can be concave, but it should not twist on itself
  // The points should be defined in clockwise order due to the rendering
  // implementation. Counter-clockwise will work in older firmwares, but
  // it is not officially supported
  (GPoint []) {
    {-3, 125},
    {3, 125},
    {3, -125},
	{-3, -125}
  }
};

enum MessageKeys {
	MK_BACKGROUND_COLOR = 0,
	MK_HOUR_COLOR = 1,
	MK_HAND_COLOR = 2,
	MK_DOT_COLOR = 3,
	MK_HAND_OUTLINE_COLOR = 4,
	MK_HAND_OUTLINE_BOOL = 5
};

const int midWidth = 47;
const int midHeight = 59;
const int screenMidWidth = 72;
const int screenMidHeight = 84;
const int radius = 250;
const int clockUnit = 30;

static Window *s_main_window;

static TextLayer * s_time_layer;
static TextLayer * s_time_layer2;

static Layer *s_path_layer;
static GPath *s_line_path;

static Layer *s_dot_layer;

static int s_path_angle;
static double s_path_angle_adj_rad;
static int s_hour_angle;
static double s_hour_angle_adj_rad;

static GColor backgroundColor;
static GColor handColor;
static GColor handBorderColor;
static GColor dotColor;
static GColor hourColor;
static bool handBorderToggle;

static double getCos(double angle) {		
	return ( (double) cos_lookup(angle * TRIG_MAX_ANGLE / (2 * M_PI)) / (double) TRIG_MAX_RATIO);
}

static double getSin(double angle) {
		return ( (double) sin_lookup(angle * TRIG_MAX_ANGLE / (2 * M_PI)) / (double) TRIG_MAX_RATIO);
}

static GColor getColor(char* colorString) {
	if (strcmp(colorString, "blk") == 0) {
		return GColorBlack;
	}
	else if (strcmp(colorString, "wht") == 0) {
		return GColorWhite;
	}
	else if (strcmp(colorString, "red") == 0) {
		return GColorRed;
	}
	else if (strcmp(colorString, "org") == 0) {
		return GColorOrange;
	}
	else if (strcmp(colorString, "ylw") == 0) {
		return GColorYellow;
	}
	else if (strcmp(colorString, "grn") == 0) {
		return GColorGreen;
	}
	else if (strcmp(colorString, "blu") == 0) {
		return GColorBlue;
	}
	else if (strcmp(colorString, "prp") == 0) {
		return GColorPurple;
	}
	else if (strcmp(colorString, "pnk") == 0) {
		return GColorShockingPink;
	}
	else if (strcmp(colorString, "gry") == 0) {
		return GColorDarkGray;
	}
	else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "getColor received an invalid string: %s", colorString);
		return GColorWhite;
	}
}

static void in_received_handler(DictionaryIterator *received, void *ctx) {	
	Tuple *currDictItem = dict_read_first(received);
		
	while (currDictItem) {		
		if (currDictItem->key == MK_BACKGROUND_COLOR) {
			backgroundColor = getColor(currDictItem->value->cstring);
			persist_write_string(MK_BACKGROUND_COLOR, currDictItem->value->cstring);
		} 
		else if (currDictItem->key == MK_HOUR_COLOR) {
			hourColor = getColor(currDictItem->value->cstring);
			persist_write_string(MK_HOUR_COLOR, currDictItem->value->cstring);
		}
		else if (currDictItem->key == MK_HAND_COLOR) {
			handColor = getColor(currDictItem->value->cstring);
			persist_write_string(MK_HAND_COLOR, currDictItem->value->cstring);
		}
		else if (currDictItem->key == MK_DOT_COLOR) {
			dotColor = getColor(currDictItem->value->cstring);
			persist_write_string(MK_DOT_COLOR, currDictItem->value->cstring);
		}
		else if (currDictItem->key == MK_HAND_OUTLINE_COLOR) {
			if (strcmp(currDictItem->value->cstring, "nob") == 0) {
				handBorderColor = GColorBlack;
				handBorderToggle = false;
				persist_write_string(MK_HAND_OUTLINE_COLOR, "blk");
				persist_write_bool(MK_HAND_OUTLINE_BOOL, handBorderToggle);
			}
			else {
				handBorderColor = getColor(currDictItem->value->cstring);
				handBorderToggle = true;
				persist_write_string(MK_HAND_OUTLINE_COLOR, currDictItem->value->cstring);
				persist_write_bool(MK_HAND_OUTLINE_BOOL, handBorderToggle);
			}
		}
		else {
			APP_LOG(APP_LOG_LEVEL_DEBUG, "default!, %d", (int) currDictItem->key);
		}
		
		currDictItem = dict_read_next(received);
	}
	
	window_set_background_color(s_main_window, backgroundColor);
	text_layer_set_background_color(s_time_layer, backgroundColor);
	text_layer_set_background_color(s_time_layer2, backgroundColor);
	text_layer_set_text_color(s_time_layer, hourColor);
	text_layer_set_text_color(s_time_layer2, hourColor);
	
	Layer * timeLayer = text_layer_get_layer(s_time_layer);
	Layer * timeLayer2 = text_layer_get_layer(s_time_layer2);
	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(s_dot_layer);
	layer_mark_dirty(s_path_layer);	
}

void in_dropped_handler(AppMessageResult reason, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message Dropped: %d", reason);
}

// Layer update callback which is called on render updates
static void path_layer_update_callback(Layer *layer, GContext *ctx) {
	// Getting the current time
	time_t tempTime = time(NULL);
	struct tm * timeStruct = localtime(&tempTime);

	s_path_angle = (((timeStruct->tm_hour % 12) * 60) + timeStruct->tm_min) * 360 / (12 * 60);

	gpath_rotate_to(s_line_path, (TRIG_MAX_ANGLE / 360) * s_path_angle);

	graphics_context_set_stroke_color(ctx, handBorderColor);
	graphics_context_set_fill_color(ctx, handColor);
	gpath_draw_filled(ctx, s_line_path);	
	if (handBorderToggle) {
		gpath_draw_outline(ctx, s_line_path);
	}
}

static void dot_layer_update_callback(Layer *layer, GContext *ctx) {
		
	graphics_context_set_stroke_color(ctx, dotColor);
	graphics_context_set_fill_color(ctx, dotColor);
	
	time_t tempTime = time(NULL);
	struct tm * tick_time = localtime(&tempTime);
	
	s_path_angle = (((tick_time->tm_hour % 12) * 60) + tick_time->tm_min) / 2;
	s_path_angle_adj_rad = -(s_path_angle * M_PI / 180) + (M_PI / 2);
	
	s_hour_angle = (((tick_time->tm_hour % 12) * 60)) / 2;
	s_hour_angle_adj_rad = -(s_hour_angle * M_PI / 180) + (M_PI / 2);
	
	if (s_path_angle_adj_rad > (M_PI / 2) &&
	   s_path_angle_adj_rad < (3 * M_PI / 2)) {
		s_path_angle_adj_rad += (2 * M_PI);	
	}
	
	if (s_hour_angle_adj_rad > (M_PI / 2) &&
	   s_hour_angle_adj_rad < (3 * M_PI / 2)) {
		s_hour_angle_adj_rad += (2 * M_PI);	
	}
		
	double timeX = getCos(s_path_angle_adj_rad) * radius;	
	double timeY = getSin(s_path_angle_adj_rad) * radius;
		
	double preDotX = getCos(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (M_PI / 24))) * radius;
	double preDotX2 = getCos(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double preDotX3 = getCos(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (3 * M_PI / 24))) * radius;
		
	double preDotY = getSin(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (M_PI / 24))) * radius;
	double preDotY2 = getSin(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double preDotY3 = getSin(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (3 * M_PI / 24))) * radius;
		
	double postDotX = getCos(s_hour_angle_adj_rad - ((M_PI / 6) - (M_PI / 24))) * radius;
	double postDotX2 = getCos(s_hour_angle_adj_rad - ((M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postDotX3 = getCos(s_hour_angle_adj_rad - ((M_PI / 6) - (3 * M_PI / 24))) * radius;
		
	double postDotY = getSin(s_hour_angle_adj_rad - ((M_PI / 6) - (M_PI / 24))) * radius;
	double postDotY2 = getSin(s_hour_angle_adj_rad - ((M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postDotY3 = getSin(s_hour_angle_adj_rad - ((M_PI / 6) - (3 * M_PI / 24))) * radius;	
	double postPostDotX = getCos(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (M_PI / 24))) * radius;
	double postPostDotX2 = getCos(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postPostDotX3 = getCos(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (3 * M_PI / 24))) * radius;
	double postPostDotY = getSin(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (M_PI / 24))) * radius;
	double postPostDotY2 = getSin(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postPostDotY3 = getSin(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (3 * M_PI / 24))) * radius;
		
	struct GPoint preDot1, 
		preDot2, 
		preDot3, 
		postDot1, 
		postDot2, 
		postDot3, 
		postPostDot1, 
		postPostDot2, 
		postPostDot3;
	
	preDot1.x = (preDotX - timeX) + screenMidWidth;
	preDot1.y = (timeY - preDotY) + screenMidHeight;
	
	preDot2.x = (preDotX2 - timeX) + screenMidWidth;
	preDot2.y = (timeY - preDotY2) + screenMidHeight;
	
	preDot3.x = (preDotX3 - timeX) + screenMidWidth;
	preDot3.y = (timeY - preDotY3) + screenMidHeight;
	
	postDot1.x = (postDotX - timeX) + screenMidWidth;
	postDot1.y = (timeY - postDotY) + screenMidHeight;
	
	postDot2.x = (postDotX2 - timeX) + screenMidWidth;
	postDot2.y = (timeY - postDotY2) + screenMidHeight;
	
	postDot3.x = (postDotX3 - timeX) + screenMidWidth;
	postDot3.y = (timeY - postDotY3) + screenMidHeight;
	
	postPostDot1.x = (postPostDotX - timeX) + screenMidWidth;
	postPostDot1.y = (timeY - postPostDotY) + screenMidHeight;
	
	postPostDot2.x = (postPostDotX2 - timeX) + screenMidWidth;
	postPostDot2.y = (timeY - postPostDotY2) + screenMidHeight;
	
	postPostDot3.x = (postPostDotX3 - timeX) + screenMidWidth;
	postPostDot3.y = (timeY - postPostDotY3) + screenMidHeight;
	
	graphics_fill_circle(ctx, preDot1, 3);
	graphics_fill_circle(ctx, preDot2, 5);	
	graphics_fill_circle(ctx, preDot3, 3);
	graphics_fill_circle(ctx, postDot1, 3);
	graphics_fill_circle(ctx, postDot2, 5);	
	graphics_fill_circle(ctx, postDot3, 3);
	graphics_fill_circle(ctx, postPostDot1, 3);
	graphics_fill_circle(ctx, postPostDot2, 5);	
	graphics_fill_circle(ctx, postPostDot3, 3);
}

static void update_time() {		
	time_t tempTime = time(NULL);
	struct tm * tick_time = localtime(&tempTime);
	int currHour = tick_time->tm_hour;
	
	Layer * timeLayer = text_layer_get_layer(s_time_layer);
	Layer * timeLayer2 = text_layer_get_layer(s_time_layer2);
	
	char * buffer = "temp1";
	char * buffer2 = "temp2";
	
	strftime(buffer, sizeof("00"), "%I", tick_time);
	
	s_path_angle = (((tick_time->tm_hour % 12) * 60) + tick_time->tm_min) / 2;
	s_path_angle_adj_rad = -(s_path_angle * M_PI / 180) + (M_PI / 2);
	
	s_hour_angle = (((tick_time->tm_hour % 12) * 60)) / 2;
	s_hour_angle_adj_rad = -(s_hour_angle * M_PI / 180) + (M_PI / 2);
	
	if (s_path_angle_adj_rad > (M_PI / 2) &&
	   s_path_angle_adj_rad < (3 * M_PI / 2)) {
		s_path_angle_adj_rad += (2 * M_PI);	
	}
	
	if (s_hour_angle_adj_rad > (M_PI / 2) &&
	   s_hour_angle_adj_rad < (3 * M_PI / 2)) {
		s_hour_angle_adj_rad += (2 * M_PI);	
	}
	
	if (tick_time->tm_hour == 23) {
		tick_time->tm_hour = 0;
	}
	else {
		tick_time->tm_hour++;
	}
	
	strftime(buffer2, sizeof("00"), "%I", tick_time);
				
	double timeX = getCos(s_path_angle_adj_rad) * radius;	
	double timeY = getSin(s_path_angle_adj_rad) * radius;
	double hourX = getCos(s_hour_angle_adj_rad) * radius;
	double hourY = getSin(s_hour_angle_adj_rad) * radius;
	
	double hourX2 = getCos(s_hour_angle_adj_rad - (0.5236)) * radius;
	double hourY2 = getSin(s_hour_angle_adj_rad - (0.5236)) * radius;
	
	int xPos = -(timeX - hourX) + midWidth;
	int yPos = -(hourY - timeY) + midHeight;
	
	int xPos2 = -(timeX - hourX2) + midWidth;
	int yPos2 = -(hourY2 - timeY) + midHeight;
	layer_set_frame(timeLayer, GRect(xPos,yPos,50,50));
	layer_set_frame(timeLayer2, GRect(xPos2,yPos2,50,50));
		
	char * bufferS = buffer+1;
	char * buffer2S = buffer2+1;
		
	if (currHour > 0 && currHour < 10) {
		text_layer_set_text(s_time_layer, bufferS);
	}
	else if (currHour > 12 && currHour < 22) {
		text_layer_set_text(s_time_layer, bufferS);
	}
	else {
		text_layer_set_text(s_time_layer, buffer);
	}
	
	if (currHour >= 0 && currHour < 9) {
		text_layer_set_text(s_time_layer2, buffer2S);
	}
	else if (currHour >= 12 && currHour < 21) {
		text_layer_set_text(s_time_layer2, buffer2S);
	}
	else {
		text_layer_set_text(s_time_layer2, buffer2);
	}
	
	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
		
	gpath_rotate_to(s_line_path, (TRIG_MAX_ANGLE / 360) * s_path_angle);
	layer_mark_dirty(s_path_layer);	
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	update_time();
}

static void main_window_load(Window *window) {		
	time_t tempTime = time(NULL);
	struct tm * tick_time = localtime(&tempTime);
	int currHour = tick_time->tm_hour;
	
	char * buffer = "temp1";
	char * buffer2 = "temp2";
	
	strftime(buffer, sizeof("00"), "%I", tick_time);
	
	s_path_angle = (((tick_time->tm_hour % 12) * 60) + tick_time->tm_min) / 2;
	s_path_angle_adj_rad = -(s_path_angle * M_PI / 180) + (M_PI / 2);
	
	s_hour_angle = (((tick_time->tm_hour % 12) * 60)) / 2;
	s_hour_angle_adj_rad = -(s_hour_angle * M_PI / 180) + (M_PI / 2);
		
	if (s_path_angle_adj_rad > (M_PI / 2) &&
	   s_path_angle_adj_rad < (3 * M_PI / 2)) {
		s_path_angle_adj_rad += (2 * M_PI);	
	}
	
	if (s_hour_angle_adj_rad > (M_PI / 2) &&
	   s_hour_angle_adj_rad < (3 * M_PI / 2)) {
		s_hour_angle_adj_rad += (2 * M_PI);	
	}
	
	if (tick_time->tm_hour == 23) {
		tick_time->tm_hour = 0;
	}
	else {
		tick_time->tm_hour++;
	}	
		
	strftime(buffer2, sizeof("00"), "%I", tick_time);
	
	double timeX = getCos(s_path_angle_adj_rad) * radius;		
	double timeY = getSin(s_path_angle_adj_rad) * radius;
		
	double hourX = getCos(s_hour_angle_adj_rad) * radius;
	double hourY = getSin(s_hour_angle_adj_rad) * radius;
			
	double hourX2 = getCos(s_hour_angle_adj_rad - (M_PI / 6)) * radius;
	double hourY2 = getSin(s_hour_angle_adj_rad - (M_PI / 6)) * radius;
	
	int xPos = (hourX - timeX) + midWidth;
	int yPos = (timeY - hourY) + midHeight;
	
	int xPos2 = (hourX2 - timeX) + midWidth;
	int yPos2 = (timeY - hourY2) + midHeight;
	
	char * bufferS = buffer+1;
	char * buffer2S = buffer2+1;
			
	s_time_layer = text_layer_create(GRect(xPos, yPos, 50, 50));
	text_layer_set_background_color(s_time_layer, backgroundColor);
	text_layer_set_text_color(s_time_layer, hourColor);

	s_time_layer2 = text_layer_create(GRect(xPos2, yPos2, 50, 50));
	text_layer_set_background_color(s_time_layer2, backgroundColor);
	text_layer_set_text_color(s_time_layer2, hourColor);
	
	if (currHour > 0 && currHour < 10) {
		text_layer_set_text(s_time_layer, bufferS);
	}
	else if (currHour > 12 && currHour < 23) {
		text_layer_set_text(s_time_layer, bufferS);
	}
	else {
		text_layer_set_text(s_time_layer, buffer);
	}
	
	if (currHour >= 0 && currHour < 9) {
		text_layer_set_text(s_time_layer2, buffer2S);
	}
	else if (currHour >= 12 && currHour < 22) {
		text_layer_set_text(s_time_layer2, buffer2S);
	}
	else {
		text_layer_set_text(s_time_layer2, buffer2);
	}
		
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
	
	text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
		
	text_layer_set_font(s_time_layer2, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(s_time_layer2, GTextAlignmentCenter);
	
	s_dot_layer = layer_create(bounds);
	layer_set_update_proc(s_dot_layer, dot_layer_update_callback);
	
	s_path_layer = layer_create(bounds);
	layer_set_update_proc(s_path_layer, path_layer_update_callback);
	
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));		
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer2));	
	layer_add_child(window_layer, s_dot_layer);
	layer_add_child(window_layer, s_path_layer);	
				
	// Move all paths to the center of the screen
	gpath_move_to(s_line_path, GPoint(bounds.size.w/2, bounds.size.h/2));
	
}

static void main_window_unload(Window *window) {
	layer_destroy(s_path_layer);
	layer_destroy(s_dot_layer);
}

static void init() {	
	s_line_path = gpath_create(&LINE_PATH_POINTS);
	
	char * strBuffer = "blu";
	
	if (persist_exists(MK_BACKGROUND_COLOR)) {
		persist_read_string(MK_BACKGROUND_COLOR, strBuffer, sizeof(strBuffer));
		backgroundColor = getColor(strBuffer);
	}
	else {
		backgroundColor = GColorBlack;
	}
	
	if (persist_exists(MK_HOUR_COLOR)) {
		persist_read_string(MK_HOUR_COLOR, strBuffer, sizeof(strBuffer));
		hourColor = getColor(strBuffer);
	}
	else {
		hourColor = GColorWhite;
	}
	
	if (persist_exists(MK_HAND_COLOR)) {
		persist_read_string(MK_HAND_COLOR, strBuffer, sizeof(strBuffer));
		handColor = getColor(strBuffer);
	}
	else {
		handColor = GColorWhite;
	}
	
	if (persist_exists(MK_DOT_COLOR)) {
		persist_read_string(MK_DOT_COLOR, strBuffer, sizeof(strBuffer));
		dotColor = getColor(strBuffer);
	}
	else {
		dotColor = GColorWhite;
	}
	
	if (persist_exists(MK_HAND_OUTLINE_COLOR)) {
		persist_read_string(MK_HAND_OUTLINE_COLOR, strBuffer, sizeof(strBuffer));
		handBorderColor = getColor(strBuffer);
	}
	else {
		handBorderColor = GColorBlack;
	}
	
	if (persist_exists(MK_HAND_OUTLINE_BOOL)) {
		handBorderToggle = persist_read_bool(MK_HAND_OUTLINE_BOOL);
	}
	else {
		handBorderToggle = true;
	}
	
	// Create Window
	s_main_window = window_create();
	window_set_background_color(s_main_window, backgroundColor);
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload,
	});
	
	window_stack_push(s_main_window, true);
	
	// Pass the corresponding GPathInfo to initialize a GPath
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_open(128, 128);
	
	Layer * timeLayer = text_layer_get_layer(s_time_layer);
	Layer * timeLayer2 = text_layer_get_layer(s_time_layer2);
	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(s_dot_layer);
	layer_mark_dirty(s_path_layer);	
}

static void deinit() {
	window_destroy(s_main_window);

	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	
	gpath_destroy(s_line_path);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}

