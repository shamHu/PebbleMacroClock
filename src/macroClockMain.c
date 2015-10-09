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

static const GPathInfo TOP_LINE_POINTS = {
	2,
	(GPoint []) {
		{0, 24},
		{144, 24}
	}
};

static const GPathInfo BOT_LINE_POINTS = {
	2,
	(GPoint []) {
		{0, 143},
		{144, 143}
	}
};

enum MessageKeys {
	MK_BACKGROUND_COLOR = 0,
	MK_HOUR_COLOR = 1,
	MK_HAND_COLOR = 2,
	MK_DOT_COLOR = 3,
	MK_HAND_OUTLINE_COLOR = 4,
	MK_HAND_OUTLINE_BOOL = 99,
	MK_VIBE_TOGGLE = 5,
	MK_HOUR_FORMAT = 6,
	MK_VIBE_START = 7,
	MK_VIBE_END = 8,
	MK_DATE_TOGGLE = 9,
	MK_DIG_TIME_TOGGLE = 10,
	MK_BT_ALERT_TOGGLE = 11
};

enum DateToggle {
	DT_OFF = 0,
	DT_FLICK = 1,
	DT_ALWAYS_ON = 2
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
static TextLayer * s_date_layer;
static TextLayer * s_date_layer2;

static Layer * s_path_layer;
static GPath * s_line_path;

static Layer * topPathLayer;
static GPath * topLinePath;

static Layer * botPathLayer;
static GPath * botLinePath;
	
static Layer * timeLayer;
static Layer * timeLayer2;
static Layer * dateLayer;
static Layer * dateLayer2;

static Layer * s_dot_layer;

static int s_path_angle;
static double s_path_angle_adj_rad;
static int s_hour_angle;
static double s_hour_angle_adj_rad;

static char buffer[2];
static char buffer2[2];
static char dateBuffer[16];
static char dateBuffer2[8];

static GColor backgroundColor;
static GColor handColor;
static GColor handBorderColor;
static GColor dotColor;
static GColor hourColor;
static bool handBorderToggle;

static bool vibeToggle;
static int vibeStartTime;
static int vibeEndTime;

static bool hourFormat;

static int dateToggle; //0 = Off, 1 = Flick, 2 = Always On
static int digTimeToggle;

static bool btAlertToggle;

static double getCos(double angle) {		
	return ( (double) cos_lookup(angle * TRIG_MAX_ANGLE / (2 * M_PI)) / (double) TRIG_MAX_RATIO);
}

static double getSin(double angle) {
	return ( (double) sin_lookup(angle * TRIG_MAX_ANGLE / (2 * M_PI)) / (double) TRIG_MAX_RATIO);
}

static char* getDay(int dayInt) {
	if (dayInt == 0) {
		return "Sun";
	}
	else if (dayInt == 1) {
		return "Mon";
	}
	else if (dayInt == 2) {
		return "Tue";
	}
	else if (dayInt == 3) {
		return "Wed";
	}
	else if (dayInt == 4) {
		return "Thu";
	}
	else if (dayInt == 5) {
		return "Fri";
	}
	else if (dayInt == 6) {
		return "Sat";
	}
	else {
		return "Err";
	}
}

static char* getMonth(int monthInt) {
	if (monthInt == 0) {
		return "Jan";
	}
	else if (monthInt == 1) {
		return "Feb";
	}
	else if (monthInt == 2) {
		return "Mar";
	}
	else if (monthInt == 3) {
		return "Apr";
	}
	else if (monthInt == 4) {
		return "May";
	}
	else if (monthInt == 5) {
		return "Jun";
	}
	else if (monthInt == 6) {
		return "Jul";
	}
	else if (monthInt == 7) {
		return "Aug";
	}
	else if (monthInt == 8) {
		return "Sep";
	}
	else if (monthInt == 9) {
		return "Oct";
	}
	else if (monthInt == 10) {
		return "Nov";
	}
	else if (monthInt == 11) {
		return "Dec";
	}
	else {
		return "Err";
	}
}

static int getHourInt(char* hourString) {
	int toReturn = hourString[1] - '0';
	if (hourString[0] == '1') {
		toReturn += 10;
	}
	
	if (hourString[2] == 'p') {
		toReturn += 12;
	}
	
	if (toReturn >= 0 && toReturn <= 23) {
		return toReturn;
	}
	else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "getHourInt(char*) returned an invalid integer value");
		return 0;
	}
}

static int getToggleInt(char* intString) {
	if (intString[0] == '0') {
		return 0;
	}
	else if (intString[0] == '1') {
		return 1;
	}
	else if (intString[0] == '2') {
		return 2;
	}
	else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "getToggleInt() returned invalid value: %s", intString);
		return -1;
	}
		
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
		return GColorDarkGreen;
	}
	else if (strcmp(colorString, "ble") == 0) {
		return GColorDukeBlue;
	}
	else if (strcmp(colorString, "prp") == 0) {
		return GColorImperialPurple;
	}
	else if (strcmp(colorString, "pnk") == 0) {
		return GColorShockingPink;
	}
	else if (strcmp(colorString, "gry") == 0) {
		return GColorDarkGray;
	}
	else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "getColor received an invalid string: %s", colorString);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "strcmp: %d", strcmp(colorString, "blu"));

		return GColorWhite;
	}
}

void in_dropped_handler(AppMessageResult reason, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message Dropped: %d", reason);
}

static void top_line_layer_update_callback(Layer *layer, GContext *ctx) {
	graphics_context_set_stroke_color(ctx, dotColor);
	gpath_draw_outline(ctx, topLinePath);
}

static void bot_line_layer_update_callback(Layer *layer, GContext *ctx) {
	graphics_context_set_stroke_color(ctx, dotColor);
	gpath_draw_outline(ctx, botLinePath);
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
	
	strftime(dateBuffer, sizeof(dateBuffer), "%a, %b %e", tick_time);
		
	if (hourFormat) {
		strftime(buffer, sizeof("00"), "%H", tick_time);
		strftime(dateBuffer2, sizeof("00:00"), "%H:%M", tick_time);
	}
	else {
		strftime(buffer, sizeof("00"), "%I", tick_time);
		strftime(dateBuffer2, sizeof("00:00 XX"), "%I:%M %p", tick_time);
	}
		
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
	
	if (hourFormat) {
		strftime(buffer2, sizeof("00"), "%H", tick_time);
	}
	else {
		strftime(buffer2, sizeof("00"), "%I", tick_time);
	}
				
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
	
	//xPos-5 and width=60 because "20" doesn't fit in 50x50 apparently.
	layer_set_frame(timeLayer, GRect(xPos-5,yPos,60,50));
	layer_set_frame(timeLayer2, GRect(xPos2-5,yPos2,60,50));
		
	char * bufferS = buffer+1;
	char * buffer2S = buffer2+1;
	
	if (hourFormat) {
		if (currHour > 9) {
			text_layer_set_text(s_time_layer, buffer);
			if (currHour == 23) {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2);
			}
		}
		else {
			text_layer_set_text(s_time_layer, bufferS);
			
			if (currHour == 9) {
				text_layer_set_text(s_time_layer2, buffer2);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
		}
	}
	else {
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
	}
	
	if (tick_time->tm_min == 0) {
		if (vibeToggle) {
			if (vibeEndTime < vibeStartTime) {
				if (currHour >= vibeStartTime || currHour <= vibeEndTime) {
					vibes_double_pulse();
				}
			}
			else {	
				if (currHour >= vibeStartTime && currHour <= vibeEndTime) {
		    		vibes_double_pulse();
				}
			}
		}
	}
	
	text_layer_set_text(s_date_layer, dateBuffer);
	text_layer_set_text(s_date_layer2, dateBuffer2);
	
	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(dateLayer);
	layer_mark_dirty(dateLayer2);
			
	gpath_rotate_to(s_line_path, (TRIG_MAX_ANGLE / 360) * s_path_angle);
	layer_mark_dirty(s_path_layer);	
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	update_time();
}

static void hideDate(void *data) {
	layer_set_hidden(data, true);
}


static void tap_handler(AccelAxisType axis, int32_t direction) {
	if (dateToggle == DT_FLICK) {
		layer_set_hidden(dateLayer, false);
		layer_set_hidden(topPathLayer, false);
		app_timer_register(3500, hideDate, dateLayer);
		app_timer_register(3500, hideDate, topPathLayer);
	}
	
	if (digTimeToggle == DT_FLICK) {
		layer_set_hidden(dateLayer2, false);
		layer_set_hidden(botPathLayer, false);
		app_timer_register(3500, hideDate, dateLayer2);	
		app_timer_register(3500, hideDate, botPathLayer);
	}	
}

static void bt_handler(bool connected) {
	if (btAlertToggle) {
		if (!connected) {
			vibes_long_pulse();
		}
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
		else if (currDictItem->key == MK_VIBE_TOGGLE) {
			if (strcmp(currDictItem->value->cstring, "onn") == 0) {
				vibeToggle = true;
				persist_write_bool(MK_VIBE_TOGGLE, true);
			}
			else {
				vibeToggle = false;
				persist_write_bool(MK_VIBE_TOGGLE, false);
			}
		}
		else if (currDictItem->key == MK_HOUR_FORMAT) {
			if (strcmp(currDictItem->value->cstring, "24h") == 0) {
				hourFormat = true;
				persist_write_bool(MK_HOUR_FORMAT, true);
			}
			else {
				hourFormat = false;
				persist_write_bool(MK_HOUR_FORMAT, false);
			}
		}
		else if (currDictItem->key == MK_VIBE_START) {
			vibeStartTime = getHourInt(currDictItem->value->cstring);
			persist_write_string(MK_VIBE_START, currDictItem->value->cstring);
		}
		else if (currDictItem->key == MK_VIBE_END) {
			vibeEndTime = getHourInt(currDictItem->value->cstring);
			persist_write_string(MK_VIBE_END, currDictItem->value->cstring);
		}
		else if (currDictItem->key == MK_DATE_TOGGLE) {
			dateToggle = getToggleInt(currDictItem->value->cstring);
			persist_write_int(MK_DATE_TOGGLE, dateToggle);
		}
		else if (currDictItem->key == MK_DIG_TIME_TOGGLE) {
			digTimeToggle = getToggleInt(currDictItem->value->cstring);
			persist_write_int(MK_DIG_TIME_TOGGLE, digTimeToggle);
		}
		else if (currDictItem->key == MK_BT_ALERT_TOGGLE) {
			if (strcmp(currDictItem->value->cstring, "onn") == 0) {
				btAlertToggle = true;
				persist_write_bool(MK_BT_ALERT_TOGGLE, true);
			}
			else {
				btAlertToggle = false;
				persist_write_bool(MK_BT_ALERT_TOGGLE, false);
			}
		}
		else {
			APP_LOG(APP_LOG_LEVEL_DEBUG, "default!, %d", (int) currDictItem->key);
		}
		
		currDictItem = dict_read_next(received);
	}
	
	if (dateToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer, false);
		layer_set_hidden(topPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer, true);
		layer_set_hidden(topPathLayer, true);
	}
		
	if (digTimeToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer2, false);
		layer_set_hidden(botPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer2, true);
		layer_set_hidden(botPathLayer, true);
	}
	
	window_set_background_color(s_main_window, backgroundColor);
	text_layer_set_background_color(s_time_layer, backgroundColor);
	text_layer_set_background_color(s_time_layer2, backgroundColor);
	text_layer_set_background_color(s_date_layer, backgroundColor);
	text_layer_set_background_color(s_date_layer2, backgroundColor);
	text_layer_set_text_color(s_date_layer, hourColor);
	text_layer_set_text_color(s_date_layer2, hourColor);
	text_layer_set_text_color(s_time_layer, hourColor);
	text_layer_set_text_color(s_time_layer2, hourColor);
	
	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(s_dot_layer);
	layer_mark_dirty(s_path_layer);	
	layer_mark_dirty(dateLayer);
	layer_mark_dirty(dateLayer2);
	layer_mark_dirty(topPathLayer);
	layer_mark_dirty(botPathLayer);
	update_time();
}

static void main_window_load(Window *window) {		
	time_t tempTime = time(NULL);
	struct tm * tick_time = localtime(&tempTime);
	int currHour = tick_time->tm_hour;
	
	strftime(dateBuffer, sizeof(dateBuffer), "%a, %b %e", tick_time);
		
	if (hourFormat) {
		strftime(buffer, sizeof("00"), "%H", tick_time);
		strftime(dateBuffer2, sizeof("00:00"), "%H:%M", tick_time);
	}
	else {
		strftime(buffer, sizeof("00"), "%I", tick_time);
		strftime(dateBuffer2, sizeof("00:00 XX"), "%I:%M %p", tick_time);
	}
	
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
		
	if (hourFormat) {
		strftime(buffer2, sizeof("00"), "%H", tick_time);
	}
	else {
		strftime(buffer2, sizeof("00"), "%I", tick_time);
	}
	
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
			
	s_time_layer = text_layer_create(GRect(xPos-5, yPos, 60, 50));
	text_layer_set_background_color(s_time_layer, backgroundColor);
	text_layer_set_text_color(s_time_layer, hourColor);

	s_time_layer2 = text_layer_create(GRect(xPos2-5, yPos2, 60, 50));
	text_layer_set_background_color(s_time_layer2, backgroundColor);
	text_layer_set_text_color(s_time_layer2, hourColor);
	
	s_date_layer = text_layer_create(GRect(0, 0, 144, 24));
	text_layer_set_background_color(s_date_layer, backgroundColor);
	text_layer_set_text_color(s_date_layer, hourColor);
	text_layer_set_text(s_date_layer, dateBuffer);
	
	s_date_layer2 = text_layer_create(GRect(0, 144, 144, 24));
	text_layer_set_background_color(s_date_layer2, backgroundColor);
	text_layer_set_text_color(s_date_layer2, hourColor);
	text_layer_set_text(s_date_layer2, dateBuffer2);

		
	if (hourFormat) {
		if (currHour > 9) {
			text_layer_set_text(s_time_layer, buffer);
			if (tick_time->tm_hour == 0) {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2);
			}
		}
		else {
			text_layer_set_text(s_time_layer, bufferS);
			
			if (currHour == 9) {
				text_layer_set_text(s_time_layer2, buffer2);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
		}
	}
	else {
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
	}
			
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
	
	text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
		
	text_layer_set_font(s_time_layer2, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(s_time_layer2, GTextAlignmentCenter);
	
	text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
	
	text_layer_set_font(s_date_layer2, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_date_layer2, GTextAlignmentCenter);
	
	s_dot_layer = layer_create(bounds);
	layer_set_update_proc(s_dot_layer, dot_layer_update_callback);
	
	s_path_layer = layer_create(bounds);
	layer_set_update_proc(s_path_layer, path_layer_update_callback);
	
	topPathLayer = layer_create(bounds);
	layer_set_update_proc(topPathLayer, top_line_layer_update_callback);
	
	botPathLayer = layer_create(bounds);
	layer_set_update_proc(botPathLayer, bot_line_layer_update_callback);
	
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));		
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer2));
	
	layer_add_child(window_layer, s_dot_layer);
	layer_add_child(window_layer, s_path_layer);
	
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer2));
	
	
	layer_add_child(window_layer, topPathLayer);
	layer_add_child(window_layer, botPathLayer);
	
	// Move all paths to the center of the screen
	gpath_move_to(s_line_path, GPoint(bounds.size.w/2, bounds.size.h/2));
}

static void main_window_unload(Window *window) {
	layer_destroy(s_path_layer);
	layer_destroy(s_dot_layer);
	layer_destroy(topPathLayer);
	layer_destroy(botPathLayer);
	layer_destroy(timeLayer);
	layer_destroy(timeLayer2);
	layer_destroy(dateLayer);
	layer_destroy(dateLayer2);
}

static void init() {	
	s_line_path = gpath_create(&LINE_PATH_POINTS);
	topLinePath = gpath_create(&TOP_LINE_POINTS);
	botLinePath = gpath_create(&BOT_LINE_POINTS);
	
	char * strBuffer = "000";
	
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
	
	if (persist_exists(MK_VIBE_TOGGLE)) {
		vibeToggle = persist_read_bool(MK_VIBE_TOGGLE);
	}
	else {
		vibeToggle = false;
	}
	
	if (persist_exists(MK_HOUR_FORMAT)) {
		hourFormat = persist_read_bool(MK_HOUR_FORMAT);
	}
	else {
		hourFormat = false;
	}
	
	if (persist_exists(MK_VIBE_START)) {
		persist_read_string(MK_VIBE_START, strBuffer, sizeof(strBuffer));
		vibeStartTime = getHourInt(strBuffer);
	}
	else {
		vibeStartTime = 8;
	}
	
	if (persist_exists(MK_VIBE_END)) {
		persist_read_string(MK_VIBE_END, strBuffer, sizeof(strBuffer));
		vibeEndTime = getHourInt(strBuffer);
	}
	else {
		vibeEndTime = 23;
	}
	
	if (persist_exists(MK_DATE_TOGGLE)) {
		dateToggle = persist_read_int(MK_DATE_TOGGLE);
	}
	else {
		dateToggle = 1;
	}
	
	if (persist_exists(MK_DIG_TIME_TOGGLE)) {
		digTimeToggle = persist_read_int(MK_DIG_TIME_TOGGLE);
	}
	else {
		digTimeToggle = 1;
	}
	
	if (persist_exists(MK_BT_ALERT_TOGGLE)) {
		btAlertToggle = persist_read_bool(MK_BT_ALERT_TOGGLE);
	}
	else {
		btAlertToggle = false;
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
	
	timeLayer = text_layer_get_layer(s_time_layer);
	timeLayer2 = text_layer_get_layer(s_time_layer2);
	dateLayer = text_layer_get_layer(s_date_layer);
	dateLayer2 = text_layer_get_layer(s_date_layer2);
	
	if (dateToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer, false);
		layer_set_hidden(topPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer, true);
		layer_set_hidden(topPathLayer, true);
	}
		
	if (digTimeToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer2, false);
		layer_set_hidden(botPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer2, true);
		layer_set_hidden(botPathLayer, true);
	}
	
	layer_insert_above_sibling(topPathLayer, dateLayer);
	layer_insert_above_sibling(botPathLayer, dateLayer2);
	
	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(s_dot_layer);
	layer_mark_dirty(s_path_layer);	
	layer_mark_dirty(topPathLayer);
	layer_mark_dirty(botPathLayer);
	layer_mark_dirty(dateLayer);
	layer_mark_dirty(dateLayer2);
	
	accel_tap_service_subscribe(tap_handler);
	bluetooth_connection_service_subscribe(bt_handler);
	
}

static void deinit() {
	window_destroy(s_main_window);

	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	accel_tap_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	
	gpath_destroy(s_line_path);
	gpath_destroy(topLinePath);
	gpath_destroy(botLinePath);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}

