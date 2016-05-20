/**
 * SECTION:element-merger
 *
 * Merges 2 videos
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
    gst-launch-1.0 ... TODO ...
 * ]|
 * </refsect2>
 */

/*

Features:

- Dynamic negotiation of caps:

  - Sink pads take their caps from upstream (if possible)
  - Source pad take its caps from downstream (if possible)

-


 */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <gst/gst.h>

#include "gstplugin.h"

GST_DEBUG_CATEGORY_STATIC (gst_merger_debug);
#define GST_CAT_DEFAULT gst_merger_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SYNC,
  PROP_SILENT
};

static GstStaticPadTemplate sinkl_factory = GST_STATIC_PAD_TEMPLATE ("sinkl",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate sinkr_factory = GST_STATIC_PAD_TEMPLATE ("sinkr",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );


static GstStaticPadTemplate srcv_factory = GST_STATIC_PAD_TEMPLATE ("srcv",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticCaps gst_merger_vin_format_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB"));

static GstStaticCaps gst_merger_vout_format_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGBA"));


static GstCaps *
gst_merger_get_vin_capslist (void)
{
  static GstCaps *caps = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_merger_vin_format_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}

static GstCaps *
gst_merger_get_vout_capslist (void)
{
  static GstCaps *caps = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_merger_vout_format_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}


static GstPadTemplate *
gst_merger_src_template_factory (void)
{
  return gst_pad_template_new ("srcv", GST_PAD_SRC, GST_PAD_ALWAYS,
      gst_merger_get_vin_capslist ());
}

static GstPadTemplate *
gst_merger_sinkl_template_factory (void)
{
  return gst_pad_template_new ("sink_l", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_merger_get_vout_capslist ());
}

static GstPadTemplate *
gst_merger_sinkr_template_factory (void)
{
  return gst_pad_template_new ("sink_r", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_merger_get_vout_capslist ());
}


#define gst_merger_parent_class parent_class

G_DEFINE_TYPE (GstMerger, gst_merger, GST_TYPE_ELEMENT);

static void
gst_merger_finalize (GObject * object)
{
  GList *channels = NULL;
  GstMerger *merger = GST_MERGER (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* initialize the merger's class */
static void
gst_merger_class_init (GstMergerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_INFO_OBJECT (klass, "");

  gobject_class->finalize = gst_merger_finalize;

  gst_element_class_set_details_simple (gstelement_class,
      "C Merger", "Generic", "Processing Element(s)", "<cJ@zougloub.eu>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_merger_src_template_factory ());
  gst_element_class_add_pad_template (gstelement_class,
      gst_merger_sinkl_template_factory ());
  gst_element_class_add_pad_template (gstelement_class,
      gst_merger_sinkr_template_factory ());
}


static gboolean
merger_init (GstPlugin * merger)
{
  GST_DEBUG_CATEGORY_INIT (gst_merger_debug, "merger", 0, "merger plugin");

  GST_INFO_OBJECT (merger, "");

  return gst_element_register (merger, "merger_c", GST_RANK_NONE,
      GST_TYPE_MERGER);
}

static void
process_eos (GstMerger * filter)
{
  GST_INFO_OBJECT (filter, "TODO free buffers");
  gst_pad_push_event (filter->srcv_pad, gst_event_new_eos ());
}

static gboolean
gst_merger_setcaps_srcv (GstMerger * merger, GstCaps * caps)
{
  GstStructure *structure;
  int rate, channels;
  gboolean ret;
  GstCaps *outcaps;
  GstCaps *othercaps;

  GST_DEBUG_OBJECT (merger, "the caps %" GST_PTR_FORMAT, caps);

  othercaps = gst_pad_get_allowed_caps (merger->srcv_pad);
  GST_DEBUG_OBJECT (merger, "other caps %" GST_PTR_FORMAT, othercaps);
  outcaps = gst_caps_copy (othercaps);
  gst_caps_unref (othercaps);

  // use framerate from source
  GstStructure *s_snk = gst_caps_get_structure (caps, 0);
  GstStructure *s_src = gst_caps_get_structure (outcaps, 0);

  gint num, den;
  gst_structure_get_fraction (s_snk, "framerate", &num, &den);
  gst_structure_set (s_src, "framerate", GST_TYPE_FRACTION, num, den, NULL);

  ret = gst_pad_set_caps (merger->srcv_pad, outcaps);

  GST_DEBUG_OBJECT (merger, "srcv caps %" GST_PTR_FORMAT, outcaps);

  gst_pad_use_fixed_caps (merger->srcv_pad);


  return ret;
}

static gboolean
gst_merger_event_srcv (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMerger *filter = GST_MERGER (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "got event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {

    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      GST_INFO_OBJECT (pad, "event caps are %" GST_PTR_FORMAT, caps);

      break;
    }

    case GST_EVENT_EOS:
      process_eos (filter);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &filter->s_segment);
      GST_INFO_OBJECT (filter, "subtitle segment: %" GST_SEGMENT_FORMAT,
          &filter->s_segment);
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


static gboolean
gst_merger_event_sinkl (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMerger *filter = GST_MERGER (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "got event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {

    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      GST_DEBUG_OBJECT (pad, "event caps are %" GST_PTR_FORMAT, caps);
      ret = gst_merger_setcaps_srcv (filter, caps);

      break;
    }

    case GST_EVENT_EOS:
      process_eos (filter);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &filter->s_segment);
      GST_INFO_OBJECT (filter, "subtitle segment: %" GST_SEGMENT_FORMAT,
          &filter->s_segment);
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


static void process_video(GstMerger * merger)
{

  GST_INFO_OBJECT(merger, "");

  while (1) {
	  GstBuffer * buf_l = NULL;
	  GstBuffer * buf_r = NULL;

	  if (g_queue_is_empty(&merger->bufs_l)) {
		  GST_INFO_OBJECT(merger, "No L buffers left");
		  return;
	  }
	  if (g_queue_is_empty(&merger->bufs_r)) {
		  GST_INFO_OBJECT(merger, "No R buffers left");
		  return;
	  }

	  GST_OBJECT_LOCK (merger);

	  GstBuffer * buf_l0 = g_queue_peek_head(&merger->bufs_l);
	  GstBuffer * buf_l1 = g_queue_peek_tail(&merger->bufs_l);
	  GstBuffer * buf_r0 = g_queue_peek_head(&merger->bufs_r);
	  GstBuffer * buf_r1 = g_queue_peek_tail(&merger->bufs_r);

	  GST_INFO_OBJECT(merger, "L0: %" GST_PTR_FORMAT, buf_l0);
	  GST_INFO_OBJECT(merger, "L1: %" GST_PTR_FORMAT, buf_l1);
	  GST_INFO_OBJECT(merger, "R0: %" GST_PTR_FORMAT, buf_r0);
	  GST_INFO_OBJECT(merger, "R1: %" GST_PTR_FORMAT, buf_r1);

	  if (GST_BUFFER_PTS(buf_l0) >= GST_BUFFER_PTS(buf_r0)
	      && GST_BUFFER_PTS(buf_l0) <= GST_BUFFER_PTS(buf_r0)
	      + GST_BUFFER_DURATION(buf_r0)) {
		  buf_l = g_queue_pop_head(&merger->bufs_l);
		  buf_r = g_queue_pop_head(&merger->bufs_r);
	  }
	  else if (GST_BUFFER_PTS(buf_r0) >= GST_BUFFER_PTS(buf_l0)
	      && GST_BUFFER_PTS(buf_r0) <= GST_BUFFER_PTS(buf_l0)
	      + GST_BUFFER_DURATION(buf_l0)) {
		  buf_l = g_queue_pop_head(&merger->bufs_l);
		  buf_r = g_queue_pop_head(&merger->bufs_r);
	  }
	  else if (GST_BUFFER_PTS(buf_r0) + GST_BUFFER_DURATION(buf_r0) < GST_BUFFER_PTS(buf_l0)) {
		  buf_r = g_queue_pop_head(&merger->bufs_r);
		  gst_buffer_unref(buf_r);
		  buf_r = NULL;
		  GST_INFO_OBJECT(merger, "Discarding one R");
		  GST_OBJECT_UNLOCK (merger);
		  continue;
	  }
	  else if (GST_BUFFER_PTS(buf_l0) + GST_BUFFER_DURATION(buf_l0) < GST_BUFFER_PTS(buf_r0)) {
		  buf_l = g_queue_pop_head(&merger->bufs_l);
		  gst_buffer_unref(buf_l);
		  buf_l = NULL;
		  GST_INFO_OBJECT(merger, "Discarding one L");
		  GST_OBJECT_UNLOCK (merger);
		  continue;
	  }
	  else {
		  GST_INFO_OBJECT(merger, "Not implemented");
	  }

	  if (buf_l == NULL || buf_r == NULL) {
		  break;
	  }

	  GST_OBJECT_UNLOCK (merger);

	  GST_INFO_OBJECT(merger, "Pushing new buffer");


	  gint w = 1280;
	  gint h = 480;
	  {
		  GstCaps * caps = gst_pad_get_current_caps (merger->srcv_pad);
		  GstStructure *s = gst_caps_get_structure (caps, 0);
		  gst_structure_get_int (s, "width", &w);
		  gst_structure_get_int (s, "height", &h);
	  }


	  GstBuffer * buf_out = gst_buffer_new_allocate(NULL, w*h*4, NULL);

	  GstMapInfo map_l;
	  GstMapInfo map_r;
	  GstMapInfo map_out;
	  bool res = gst_buffer_map (buf_out, &map_out, GST_MAP_WRITE);
	  g_assert (res);
	  res = gst_buffer_map (buf_l, &map_l, GST_MAP_READ);
	  g_assert (res);
	  res = gst_buffer_map (buf_r, &map_r, GST_MAP_READ);
	  g_assert (res);

	  guint8 * a_o = (guint8*)map_out.data;
	  guint8 * a_l = (guint8*)map_l.data;
	  guint8 * a_r = (guint8*)map_r.data;
	  for (int y = 0; y < h; y++) {
		  for (int x = 0; x < w/2; x++) {
			  guint8 r = a_l[3*(y*w/2+x)+0];
			  guint8 g = a_l[3*(y*w/2+x)+1];
			  guint8 b = a_l[3*(y*w/2+x)+2];
			  a_o[4*(y*w+x)+0] = r;
			  a_o[4*(y*w+x)+1] = g;
			  a_o[4*(y*w+x)+2] = b;
			  a_o[4*(y*w+x)+3] = 255;
		  }
		  for (int x = 0; x < w/2; x++) {
			  guint8 r = a_r[3*(y*w/2+x)+0];
			  guint8 g = a_r[3*(y*w/2+x)+1];
			  guint8 b = a_r[3*(y*w/2+x)+2];
			  a_o[4*(y*w+x+w/2)+0] = r;
			  a_o[4*(y*w+x+w/2)+1] = g;
			  a_o[4*(y*w+x+w/2)+2] = b;
			  a_o[4*(y*w+x+w/2)+3] = 255;
		  }
	  }

	  gst_buffer_unmap (buf_l, &map_l);
	  gst_buffer_unmap (buf_r, &map_r);
	  gst_buffer_unmap (buf_out, &map_out);

	  GST_BUFFER_PTS(buf_out) = GST_BUFFER_PTS(buf_l);
	  GST_BUFFER_DTS(buf_out) = GST_BUFFER_DTS(buf_l);
	  GST_BUFFER_DURATION(buf_out) = GST_BUFFER_DURATION(buf_l);

	  gst_pad_push (merger->srcv_pad, buf_out);

	  gst_buffer_unref(buf_l);
	  gst_buffer_unref(buf_r);
  }

}

static GstFlowReturn
gst_merger_chain_sinkl (GstPad * pad, GstObject * parent, GstBuffer * buf)
{

  GstMerger *merger = GST_MERGER (parent);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (merger, "Received L %" GST_PTR_FORMAT, buf);

  GST_OBJECT_LOCK (merger);
  g_queue_push_tail (&merger->bufs_l, buf);
  GST_OBJECT_UNLOCK (merger);
  process_video(merger);


  return ret;
}



static gboolean
gst_merger_event_sinkr (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMerger *filter = GST_MERGER (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      GST_DEBUG_OBJECT (pad, "event caps are %" GST_PTR_FORMAT, caps);

      ret = gst_merger_setcaps_srcv (filter, caps);
      break;
    }

    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

static GstFlowReturn
gst_merger_chain_sinkr (GstPad * pad, GstObject * parent, GstBuffer * buf)
{

  GstMerger *merger = GST_MERGER (parent);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (merger, "Received R %" GST_PTR_FORMAT, buf);

  GST_OBJECT_LOCK (merger);
  g_queue_push_tail (&merger->bufs_r, buf);
  GST_OBJECT_UNLOCK (merger);
  process_video(merger);

  return ret;
}


static GstStateChangeReturn
gst_merger_change_state (GstElement * element, GstStateChange transition)
{
  GstMerger *filter = GST_MERGER (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_segment_init (&filter->s_segment, GST_FORMAT_UNDEFINED);
      g_queue_init (&filter->bufs_l);
      g_queue_init (&filter->bufs_r);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      GstBuffer *buf;
      while ((buf = g_queue_pop_head (&filter->bufs_l)))
        gst_buffer_unref (buf);
      while ((buf = g_queue_pop_head (&filter->bufs_r)))
        gst_buffer_unref (buf);
      break;
    }
    default:
      break;
  }
  return ret;
}


static void
gst_merger_init (GstMerger * merger)
{

  int res;

  merger->sinkl_pad =
      gst_pad_new_from_static_template (&sinkl_factory, "sink_l");
  gst_pad_set_event_function (merger->sinkl_pad,
      GST_DEBUG_FUNCPTR (gst_merger_event_sinkl));
  gst_pad_set_chain_function (merger->sinkl_pad,
      GST_DEBUG_FUNCPTR (gst_merger_chain_sinkl));
  gst_element_add_pad (GST_ELEMENT (merger), merger->sinkl_pad);

  merger->sinkr_pad =
      gst_pad_new_from_static_template (&sinkr_factory, "sink_r");
  gst_pad_set_event_function (merger->sinkr_pad,
      GST_DEBUG_FUNCPTR (gst_merger_event_sinkr));
  gst_pad_set_chain_function (merger->sinkr_pad,
      GST_DEBUG_FUNCPTR (gst_merger_chain_sinkr));
  gst_element_add_pad (GST_ELEMENT (merger), merger->sinkr_pad);

  merger->srcv_pad = gst_pad_new_from_static_template (&srcv_factory, "srcv");
  gst_pad_set_event_function (merger->srcv_pad,
      GST_DEBUG_FUNCPTR (gst_merger_event_srcv));
  gst_element_add_pad (GST_ELEMENT (merger), merger->srcv_pad);

}


/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "Merger"
#endif

/* gstreamer looks for this structure to register mergers
 *
 * exchange the string 'Template merger' with your merger description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, merger,
    "Sample video left/right merger", merger_init, "0.1", "GPL",
    "Zougloub", "http://zougloub.eu")
