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
#include <stdint.h>
#include <inttypes.h>

#include <gst/gst.h>

#include "gstplugin.h"


GST_DEBUG_CATEGORY_STATIC (gst_merger_debug);
#define GST_CAT_DEFAULT gst_merger_debug

/* Merger signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};


#define gst_merger_parent_class parent_class

G_DEFINE_TYPE (GstMerger, gst_merger, GST_TYPE_ELEMENT);

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



static void
process_eos (GstMerger * merger)
{
  GST_DEBUG_OBJECT (merger, "TODO free buffers");
  gst_pad_push_event (merger->srcv_pad, gst_event_new_eos ());
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
  GstMerger *merger = GST_MERGER (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "got source event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {

    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      GST_DEBUG_OBJECT (pad, "event caps are %" GST_PTR_FORMAT, caps);

      break;
    }

    case GST_EVENT_EOS:
      process_eos (merger);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &merger->s_segment);
      GST_DEBUG_OBJECT (merger, "segment: %" GST_SEGMENT_FORMAT,
          &merger->s_segment);
      break;
    case GST_EVENT_QOS:
    {
      GstQOSType type;
      gdouble proportion;
      GstClockTimeDiff diff;
      GstClockTime timestamp;
      gst_event_parse_qos (event, &type, &proportion, &diff, &timestamp);

      if (type == GST_QOS_TYPE_UNDERFLOW) {
        GST_INFO_OBJECT (pad,
            "got QOS event UNDERFLOW proportion %f diff %" PRIu64 " ts %"
            PRIu64, proportion, diff, timestamp);
        GST_OBJECT_LOCK (merger);

        gint num, den;
        {
          GstCaps *caps = gst_pad_get_current_caps (merger->srcv_pad);
          GstStructure *s = gst_caps_get_structure (caps, 0);
          gst_structure_get_fraction (s, "framerate", &num, &den);
          gst_caps_unref (caps);
        }

        int nb_bufs = 0.5 + diff / (1e9 * den / num);
        GST_WARNING_OBJECT (merger, "Discarding %d buffers", nb_bufs);
        g_queue_pop_head (&merger->bufs_l);
        g_queue_pop_head (&merger->bufs_r);
        GST_OBJECT_UNLOCK (merger);
      } else {
        GST_WARNING_OBJECT (pad, "QOS type %d not implemented", type);
        ret = gst_pad_event_default (pad, parent, event);
      }
    }
      break;

    default:
      GST_INFO_OBJECT (pad, "got source event %s", GST_EVENT_TYPE_NAME (event));
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


static gboolean
gst_merger_event_sinkl (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMerger *merger = GST_MERGER (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "got event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {

    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      GST_DEBUG_OBJECT (pad, "event caps are %" GST_PTR_FORMAT, caps);
      ret = gst_merger_setcaps_srcv (merger, caps);

      break;
    }

    case GST_EVENT_EOS:
      process_eos (merger);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &merger->s_segment);
      GST_DEBUG_OBJECT (merger, "segment: %" GST_SEGMENT_FORMAT,
          &merger->s_segment);
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


static void
process_video (GstMerger * merger)
{
  GST_DEBUG_OBJECT (merger, "Task function begin");

beg:

  GST_DEBUG_OBJECT (merger, "Task function loop");

  if ((volatile int) merger->stop) {
    GST_INFO_OBJECT (merger, "Task goto end");
    goto end;
  }


  g_mutex_trylock (&merger->task_lock);
  GST_DEBUG_OBJECT (merger, "Task waiting for cond");
  g_cond_wait (&merger->task_cond, &merger->task_lock);

  GST_DEBUG_OBJECT (merger, "Task processing");

  while (1) {
    GstBuffer *buf_l = NULL;
    GstBuffer *buf_r = NULL;

    if (GST_PAD_IS_EOS (merger->sinkl_pad)) {
      goto beg;
    }
    if (GST_PAD_IS_EOS (merger->sinkr_pad)) {
      goto beg;
    }

    if (g_queue_is_empty (&merger->bufs_l)) {
      GST_DEBUG_OBJECT (merger, "No L buffers left");
      goto beg;
    }
    if (g_queue_is_empty (&merger->bufs_r)) {
      GST_DEBUG_OBJECT (merger, "No R buffers left");
      goto beg;
    }

    GST_OBJECT_LOCK (merger);

    GstBuffer *buf_l0 = g_queue_peek_head (&merger->bufs_l);
    GstBuffer *buf_l1 = g_queue_peek_tail (&merger->bufs_l);
    GstBuffer *buf_r0 = g_queue_peek_head (&merger->bufs_r);
    GstBuffer *buf_r1 = g_queue_peek_tail (&merger->bufs_r);

    GST_DEBUG_OBJECT (merger, "L0: %" GST_PTR_FORMAT, buf_l0);
    GST_DEBUG_OBJECT (merger, "L1: %" GST_PTR_FORMAT, buf_l1);
    GST_DEBUG_OBJECT (merger, "R0: %" GST_PTR_FORMAT, buf_r0);
    GST_DEBUG_OBJECT (merger, "R1: %" GST_PTR_FORMAT, buf_r1);

    if (GST_BUFFER_PTS (buf_l0) >= GST_BUFFER_PTS (buf_r0)
        && GST_BUFFER_PTS (buf_l0) <= GST_BUFFER_PTS (buf_r0)
        + GST_BUFFER_DURATION (buf_r0)) {
      buf_l = g_queue_pop_head (&merger->bufs_l);
      buf_r = g_queue_pop_head (&merger->bufs_r);
    } else if (GST_BUFFER_PTS (buf_r0) >= GST_BUFFER_PTS (buf_l0)
        && GST_BUFFER_PTS (buf_r0) <= GST_BUFFER_PTS (buf_l0)
        + GST_BUFFER_DURATION (buf_l0)) {
      buf_l = g_queue_pop_head (&merger->bufs_l);
      buf_r = g_queue_pop_head (&merger->bufs_r);
    } else if (GST_BUFFER_PTS (buf_r0) + GST_BUFFER_DURATION (buf_r0) <
        GST_BUFFER_PTS (buf_l0)) {
      buf_r = g_queue_pop_head (&merger->bufs_r);
      gst_buffer_unref (buf_r);
      buf_r = NULL;
      GST_DEBUG_OBJECT (merger, "Discarding one R");
      GST_OBJECT_UNLOCK (merger);
      continue;
    } else if (GST_BUFFER_PTS (buf_l0) + GST_BUFFER_DURATION (buf_l0) <
        GST_BUFFER_PTS (buf_r0)) {
      buf_l = g_queue_pop_head (&merger->bufs_l);
      gst_buffer_unref (buf_l);
      buf_l = NULL;
      GST_DEBUG_OBJECT (merger, "Discarding one L");
      GST_OBJECT_UNLOCK (merger);
      continue;
    } else {
      GST_DEBUG_OBJECT (merger, "Not implemented");
    }

    if (buf_l == NULL || buf_r == NULL) {
      goto beg;
    }

    GST_OBJECT_UNLOCK (merger);

    GST_DEBUG_OBJECT (merger, "Pushing new buffer");


    gint w = 1280;
    gint h = 480;
    {
      GstCaps *caps = gst_pad_get_current_caps (merger->srcv_pad);
      GstStructure *s = gst_caps_get_structure (caps, 0);
      gst_structure_get_int (s, "width", &w);
      gst_structure_get_int (s, "height", &h);
      gst_caps_unref (caps);
    }


    GstBuffer *buf_out = gst_buffer_new_allocate (NULL, w * h * 4, NULL);

    GstMapInfo map_l;
    GstMapInfo map_r;
    GstMapInfo map_out;
    bool res = gst_buffer_map (buf_out, &map_out, GST_MAP_WRITE);
    g_assert (res);
    res = gst_buffer_map (buf_l, &map_l, GST_MAP_READ);
    g_assert (res);
    res = gst_buffer_map (buf_r, &map_r, GST_MAP_READ);
    g_assert (res);

    guint8 *a_o = (guint8 *) map_out.data;
    guint8 *a_l = (guint8 *) map_l.data;
    guint8 *a_r = (guint8 *) map_r.data;
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w / 2; x++) {
        guint8 r = a_l[3 * (y * w / 2 + x) + 0];
        guint8 g = a_l[3 * (y * w / 2 + x) + 1];
        guint8 b = a_l[3 * (y * w / 2 + x) + 2];
        a_o[4 * (y * w + x) + 0] = r;
        a_o[4 * (y * w + x) + 1] = g;
        a_o[4 * (y * w + x) + 2] = b;
        a_o[4 * (y * w + x) + 3] = 255;
      }
      for (int x = 0; x < w / 2; x++) {
        guint8 r = a_r[3 * (y * w / 2 + x) + 0];
        guint8 g = a_r[3 * (y * w / 2 + x) + 1];
        guint8 b = a_r[3 * (y * w / 2 + x) + 2];
        a_o[4 * (y * w + x + w / 2) + 0] = r;
        a_o[4 * (y * w + x + w / 2) + 1] = g;
        a_o[4 * (y * w + x + w / 2) + 2] = b;
        a_o[4 * (y * w + x + w / 2) + 3] = 255;
      }
    }

    usleep (10000);

    gst_buffer_unmap (buf_l, &map_l);
    gst_buffer_unmap (buf_r, &map_r);
    gst_buffer_unmap (buf_out, &map_out);

    GST_BUFFER_PTS (buf_out) = GST_BUFFER_PTS (buf_l);
    GST_BUFFER_DTS (buf_out) = GST_BUFFER_DTS (buf_l);
    GST_BUFFER_DURATION (buf_out) = GST_BUFFER_DURATION (buf_l);

    gst_buffer_unref (buf_l);
    gst_buffer_unref (buf_r);

    int res2 = gst_pad_push (merger->srcv_pad, buf_out);
    if (res2 != GST_FLOW_OK) {
      GST_WARNING_OBJECT (merger, "Pushing pad returned %d", res2);
      goto error;
    }

  }

  goto beg;

error:
end:

  GST_INFO_OBJECT (merger, "Task bye bye?");

  //gst_task_pause (merger->task);

  GST_INFO_OBJECT (merger, "Task bye bye");

  g_mutex_unlock (&merger->task_lock);
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
  g_cond_signal (&merger->task_cond);
  //process_video(merger);


  return ret;
}



static gboolean
gst_merger_event_sinkr (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMerger *merger = GST_MERGER (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      GST_DEBUG_OBJECT (pad, "event caps are %" GST_PTR_FORMAT, caps);

      ret = gst_merger_setcaps_srcv (merger, caps);
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
  g_cond_signal (&merger->task_cond);
  //process_video(merger);

  return ret;
}


static GstStateChangeReturn
gst_merger_change_state (GstElement * element, GstStateChange transition)
{
  GstMerger *merger = GST_MERGER (element);
  GstStateChangeReturn ret;

  GST_INFO_OBJECT (merger, "Transition %d", transition);

  switch (transition) {

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
      bool res;

      GST_INFO_OBJECT (merger, "Stoping Task");
      merger->stop = 1;
      g_cond_signal (&merger->task_cond);
      GST_INFO_OBJECT (merger, "Stoping Task.");
      res = gst_task_stop (merger->task);
      GST_INFO_OBJECT (merger, "Stoping Task.. %d", res);

      GST_INFO_OBJECT (merger, "Task state %d", merger->task->state);
      res = gst_task_join (merger->task);
      GST_INFO_OBJECT (merger, "Stoping Task... %d", res);
    }
      break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
      bool res;
      GST_INFO_OBJECT (merger, "Starting Task");
      merger->stop = 0;
      res = gst_task_start (merger->task);
      GST_INFO_OBJECT (merger, "Starting Task. %d", res);
    }
      break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_segment_init (&merger->s_segment, GST_FORMAT_UNDEFINED);
      g_queue_init (&merger->bufs_l);
      g_queue_init (&merger->bufs_r);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      GstBuffer *buf;
      while ((buf = g_queue_pop_head (&merger->bufs_l)))
        gst_buffer_unref (buf);
      while ((buf = g_queue_pop_head (&merger->bufs_r)))
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

  merger->task = gst_task_new ((GstTaskFunction) process_video, merger, NULL);
  g_cond_init (&merger->task_cond);
  g_mutex_init (&merger->task_lock);
  g_rec_mutex_init (&merger->task_mutex);
  gst_task_set_lock (merger->task, &merger->task_mutex);


}


static void
gst_merger_finalize (GObject * object)
{
  GList *channels = NULL;
  GstMerger *merger = GST_MERGER (object);

  //

  g_cond_clear (&merger->task_cond);
  g_mutex_clear (&merger->task_lock);
  gst_object_unref (merger->task);
  g_rec_mutex_clear (&merger->task_mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* initialize the merger's class */
static void
gst_merger_class_init (GstMergerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_OBJECT (klass, "");

  gobject_class->finalize = gst_merger_finalize;

  gstelement_class->change_state = gst_merger_change_state;

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

  GST_DEBUG_OBJECT (merger, "");

  return gst_element_register (merger, "merger_c", GST_RANK_NONE,
      GST_TYPE_MERGER);
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
