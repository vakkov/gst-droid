/*
 * gst-droid
 *
 * Copyright (C) 2014 Mohammed Sameer <msameer@foolab.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_DROID_CODEC_H__
#define __GST_DROID_CODEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include <OMX_Core.h>
#include <OMX_Component.h>

G_BEGIN_DECLS

#define GST_DROID_ENC_TARGET_BITRATE_DEFAULT (0xffffffff)

#define GST_TYPE_DROID_CODEC (gst_droid_codec_get_type())

/* stolen from gst-omx */
#define GST_OMX_INIT_STRUCT(st) G_STMT_START { \
  memset ((st), 0, sizeof (*(st))); \
  (st)->nSize = sizeof (*(st)); \
  (st)->nVersion.s.nVersionMajor = 1; \
  (st)->nVersion.s.nVersionMinor = 1; \
  } G_STMT_END

typedef struct _GstDroidCodec GstDroidCodec;
typedef struct _GstDroidComponent GstDroidComponent;
typedef struct _GstDroidCodecHandle GstDroidCodecHandle;
typedef struct _GstDroidComponentPort GstDroidComponentPort;

struct _GstDroidCodec
{
  GstMiniObject parent;

  GMutex lock;
  GHashTable *cores;
};

struct _GstDroidComponent
{
  GstDroidCodecHandle *handle;
  GstDroidCodec *codec;

  OMX_HANDLETYPE omx;
  GstDroidComponentPort *in_port;
  GstDroidComponentPort *out_port;
  GstElement *parent;

  GMutex lock;
  gboolean error;
  gboolean needs_reconfigure;
  gboolean started;

  GMutex full_lock;
  GCond full_cond;
  GQueue *full;

  OMX_STATETYPE state;
};

struct _GstDroidComponentPort
{
  int usage;
  //  GMutex lock;
  //  GCond cond;
  OMX_PARAM_PORTDEFINITIONTYPE def;
  GstBufferPool *buffers;
  GstAllocator *allocator;
  GstDroidComponent *comp;
};

GstDroidCodec *gst_droid_codec_get (void);

GstDroidComponent *gst_droid_codec_get_component (GstDroidCodec * codec,
						  const gchar *type, GstElement * parent);
void gst_droid_codec_destroy_component (GstDroidComponent * component);

OMX_ERRORTYPE gst_droid_codec_get_param (GstDroidComponent * comp,
					 OMX_INDEXTYPE index, gpointer param);
OMX_ERRORTYPE gst_droid_codec_set_param (GstDroidComponent * comp,
					 OMX_INDEXTYPE index, gpointer param);
OMX_ERRORTYPE gst_droid_codec_set_config (GstDroidComponent * comp,
                                         OMX_INDEXTYPE index, gpointer config);
gboolean gst_droid_codec_configure_component (GstDroidComponent *comp,
					      const GstVideoInfo * info);
gboolean gst_droid_codec_start_component (GstDroidComponent * comp, GstCaps * sink, GstCaps * src);
void gst_droid_codec_stop_component (GstDroidComponent * comp);
gboolean gst_droid_codec_set_codec_data (GstDroidComponent * comp, GstBuffer * codec_data);
gboolean gst_droid_codec_consume_frame (GstDroidComponent * comp, GstVideoCodecFrame * frame);
GstBuffer *gst_omx_buffer_get_buffer (GstDroidComponent * comp, OMX_BUFFERHEADERTYPE * buff);

gboolean gst_droid_codec_return_output_buffers (GstDroidComponent * comp);

gboolean gst_droid_codec_reconfigure_output_port (GstDroidComponent * comp);

gboolean gst_droid_codec_has_error (GstDroidComponent * comp);
gboolean gst_droid_codec_needs_reconfigure (GstDroidComponent * comp);
void gst_droid_codec_unset_needs_reconfigure (GstDroidComponent * comp);
gboolean gst_droid_codec_is_running (GstDroidComponent * comp);
void gst_droid_codec_set_running (GstDroidComponent * comp, gboolean running);

void gst_droid_codec_empty_full (GstDroidComponent * comp);
gboolean gst_droid_codec_flush (GstDroidComponent * comp, gboolean pause);
gboolean gst_droid_codec_apply_encoding_params (GstDroidComponent * comp,
    GstVideoInfo * info, GstCaps * caps, int bitrate);

void gst_droid_codec_timestamp (GstBuffer * buffer, OMX_BUFFERHEADERTYPE * buff);

const gchar *gst_omx_error_to_string (OMX_ERRORTYPE err);
const gchar *gst_omx_state_to_string (OMX_STATETYPE state);
const gchar *gst_omx_command_to_string (OMX_COMMANDTYPE cmd);

G_END_DECLS

#endif /* __GST_DROID_CODEC_H__ */
