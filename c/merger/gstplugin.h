#ifndef __GST_MERGER_H__
#define __GST_MERGER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>



G_BEGIN_DECLS
#define GST_TYPE_MERGER \
 (gst_merger_get_type())
#define GST_MERGER(obj) \
 (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MERGER,GstMerger))
#define GST_MERGER_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MERGER,GstMergerClass))
#define GST_IS_MERGER(obj) \
 (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MERGER))
#define GST_IS_MERGER_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MERGER))
#define GST_MERGER_CAST(obj)       ((GstMerger *)(obj))
typedef struct _GstMerger GstMerger;
typedef struct _GstMergerClass GstMergerClass;

struct _GstMerger
{
  GstElement base;

  GstPad *sinkl_pad;
  GstPad *sinkr_pad;
  GstPad *srcv_pad;

  GQueue bufs_l;
  GQueue bufs_r;

  GstSegment s_segment;

  GstTask *task;
  GMutex task_lock;
  GCond task_cond;
  GRecMutex task_mutex;
  int stop;
};

struct _GstMergerClass
{
  GstElementClass parent_class;
};

GType gst_merger_get_type (void);

G_END_DECLS
#endif /* __GST_MERGER_H__ */
