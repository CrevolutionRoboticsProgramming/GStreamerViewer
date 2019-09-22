#include <gst/gst.h>
#include <gst/video/video.h>

#include <cairo.h>
#include <cairo-gobject.h>

#include <glib.h>

static gboolean
on_message(GstBus* bus, GstMessage* message, gpointer user_data)
{
	GMainLoop* loop = (GMainLoop*)user_data;

	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_ERROR: {
		GError* err = NULL;
		gchar* debug;

		gst_message_parse_error(message, &err, &debug);
		g_critical("Got ERROR: %s (%s)", err->message, GST_STR_NULL(debug));
		g_main_loop_quit(loop);
		break;
	}
	case GST_MESSAGE_WARNING: {
		GError* err = NULL;
		gchar* debug;

		gst_message_parse_warning(message, &err, &debug);
		g_warning("Got WARNING: %s (%s)", err->message, GST_STR_NULL(debug));
		g_main_loop_quit(loop);
		break;
	}
	case GST_MESSAGE_EOS:
		g_main_loop_quit(loop);
		break;
	default:
		break;
	}

	return TRUE;
}

/* Datastructure to share the state we are interested in between
 * prepare and render function. */
typedef struct
{
	gboolean valid;
	GstVideoInfo vinfo;
} CairoOverlayState;

// Store the information from the caps that we are interested in.
static void
prepare_overlay(GstElement* overlay, GstCaps* caps, gpointer user_data)
{
	CairoOverlayState* state = (CairoOverlayState*)user_data;

	state->valid = gst_video_info_from_caps(&state->vinfo, caps);
}

// Draw the overlay.
static void
draw_overlay(GstElement* overlay, cairo_t* cr, int timestamp,
	int duration, gpointer user_data)
{
	CairoOverlayState* s = (CairoOverlayState*)user_data;
	int width, height;
	double lineWidth{ 0.5 };

	if (!s->valid)
		return;

	width = GST_VIDEO_INFO_WIDTH(&s->vinfo);
	height = GST_VIDEO_INFO_HEIGHT(&s->vinfo);

	cairo_set_line_width(cr, lineWidth);
	cairo_move_to(cr, width / 2, 0);
	cairo_line_to(cr, width / 2, height);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_stroke(cr);
}

// This is the important part
static GstElement*
setup_gst_pipeline(CairoOverlayState* overlay_state)
{
	GstElement* pipeline;
	GstElement* cairo_overlay;
	GstElement* udpsrc, * queue, * rtpjpegdepay, * jpegdec, * videoconvert1, * videoscale, * filter, * videoconvert2, * sink;

	// Pipeline:
	// udpsrc from RPi -> queue -> rtpjpegdepay -> jpegdec -> videoconvert -> cairooverlay -> videoscale -> capsfilter -> videoconvert -> autovideosink
	pipeline = gst_pipeline_new("gstreamer-drawing-receiver");

	udpsrc = gst_element_factory_make("udpsrc", "source");
	GstCaps* udpsrc_caps = gst_caps_new_simple("application/x-rtp",
		"encoding-name", G_TYPE_STRING, "JPEG", 
		"payload", G_TYPE_INT, 26, NULL);
	g_object_set(udpsrc, "port", 1181, NULL);
	g_object_set(udpsrc, "caps", udpsrc_caps, NULL);
	gst_caps_unref(udpsrc_caps);

	queue = gst_element_factory_make("queue", "queue");
	rtpjpegdepay = gst_element_factory_make("rtpjpegdepay", "rtpjpegdepay");
	jpegdec = gst_element_factory_make("jpegdec", "jpegdec");
	
	// We need to include the videoconvert filters because cairo only supports ARGB video
	videoconvert1 = gst_element_factory_make("videoconvert", "videoconvert1");
	cairo_overlay = gst_element_factory_make("cairooverlay", "overlay");

	videoscale = gst_element_factory_make("videoscale", "videoscale");

	filter = gst_element_factory_make("capsfilter", "filter"); 
	GstCaps* filter_caps = gst_caps_new_simple("video/x-raw",
		"width", G_TYPE_INT, 640,
		"height", G_TYPE_INT, 480, NULL);
	g_object_set(filter, "caps", filter_caps, NULL);

	videoconvert2 = gst_element_factory_make("videoconvert", "videoconvert2");
	sink = gst_element_factory_make("autovideosink", "sink");

	// If failing, the element could not be created
	g_assert(cairo_overlay);

	// Hook up the neccesary signals for cairooverlay
	g_signal_connect(cairo_overlay, "draw",
		G_CALLBACK(draw_overlay), overlay_state);
	g_signal_connect(cairo_overlay, "caps-changed",
		G_CALLBACK(prepare_overlay), overlay_state);

	gst_bin_add_many(GST_BIN(pipeline), udpsrc, queue, rtpjpegdepay, jpegdec, videoconvert1,
		cairo_overlay, videoscale, filter, videoconvert2, sink, NULL);

	if (!gst_element_link_many(udpsrc, queue, rtpjpegdepay, jpegdec, videoconvert1,
		cairo_overlay, videoscale, filter, videoconvert2, sink, NULL)) {
		g_warning("Failed to link elements!");
	}

	return pipeline;
}

int
main(int argc, char** argv)
{
	GMainLoop* loop;
	GstElement* pipeline;
	GstBus* bus;
	CairoOverlayState* overlay_state;

	gst_init(&argc, &argv);
	loop = g_main_loop_new(NULL, FALSE);

	/* allocate on heap for pedagogical reasons, makes code easier to transfer */
	overlay_state = g_new0(CairoOverlayState, 1);

	pipeline = setup_gst_pipeline(overlay_state);

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(on_message), loop);
	gst_object_unref(GST_OBJECT(bus));

	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run(loop);

	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);

	g_free(overlay_state);
	return 0;
}
