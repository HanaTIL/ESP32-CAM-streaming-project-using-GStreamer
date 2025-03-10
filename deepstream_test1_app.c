/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <cuda_runtime_api.h>
#include "gstnvdsmeta.h"

#define MAX_DISPLAY_LEN 64

#define PGIE_CLASS_ID_VEHICLE 0
#define PGIE_CLASS_ID_PERSON 2

/* The muxer output resolution must be set if the input streams will be of
 * different resolution. The muxer will scale all the input frames to this
 * resolution. */
#define MUXER_OUTPUT_WIDTH 800
#define MUXER_OUTPUT_HEIGHT 600

/* Muxer batch formation timeout, for e.g. 40 millisec. Should ideally be set
 * based on the fastest source's framerate. */
#define MUXER_BATCH_TIMEOUT_USEC 40000

gint frame_number = 0;
gchar pgie_classes_str[4][32] = { "Vehicle", "TwoWheeler", "Person",
  "Roadsign"
};

/* osd_sink_pad_buffer_probe  will extract metadata received on OSD sink pad
 * and update params for drawing rectangle, object information etc. */

static GstPadProbeReturn
osd_sink_pad_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    guint num_rects = 0; 
    NvDsObjectMeta *obj_meta = NULL;
    guint vehicle_count = 0;
    guint person_count = 0;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;
    NvDsDisplayMeta *display_meta = NULL;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta (buf);

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
      l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        int offset = 0;
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
                l_obj = l_obj->next) {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);
            if (obj_meta->class_id == PGIE_CLASS_ID_VEHICLE) {
                vehicle_count++;
                num_rects++;
            }
            if (obj_meta->class_id == PGIE_CLASS_ID_PERSON) {
                person_count++;
                num_rects++;
            }
        }
        display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
        NvOSD_TextParams *txt_params  = &display_meta->text_params[0];
        display_meta->num_labels = 1;
        txt_params->display_text = g_malloc0 (MAX_DISPLAY_LEN);
        offset = snprintf(txt_params->display_text, MAX_DISPLAY_LEN, "Person = %d ", person_count);
        offset = snprintf(txt_params->display_text + offset , MAX_DISPLAY_LEN, "Vehicle = %d ", vehicle_count);

        /* Now set the offsets where the string should appear */
        txt_params->x_offset = 10;
        txt_params->y_offset = 12;

        /* Font , font-color and font-size */
        txt_params->font_params.font_name = "Serif";
        txt_params->font_params.font_size = 10;
        txt_params->font_params.font_color.red = 1.0;
        txt_params->font_params.font_color.green = 1.0;
        txt_params->font_params.font_color.blue = 1.0;
        txt_params->font_params.font_color.alpha = 1.0;

        /* Text background color */
        txt_params->set_bg_clr = 1;
        txt_params->text_bg_clr.red = 0.0;
        txt_params->text_bg_clr.green = 0.0;
        txt_params->text_bg_clr.blue = 0.0;
        txt_params->text_bg_clr.alpha = 1.0;

        nvds_add_display_meta_to_frame(frame_meta, display_meta);
    }

    g_print ("Frame Number = %d Number of objects = %d "
            "Vehicle Count = %d Person Count = %d\n",
            frame_number, num_rects, vehicle_count, person_count);
    frame_number++;
    return GST_PAD_PROBE_OK;
}

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *error;
      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      if (debug)
        g_printerr ("Error details: %s\n", debug);
      g_free (debug);
      g_error_free (error);
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
  return TRUE;
}


static void
cb_newpad (GstElement * element, GstPad * element_src_pad, gpointer data)
{

  g_print ("In cb_newpad\n");
  GstCaps *caps = gst_pad_get_current_caps (element_src_pad);
  const GstStructure *str = gst_caps_get_structure (caps, 0);
  const gchar *name = gst_structure_get_name (str);

  GstElement * = (GstElement *) data;

  const gchar *media = gst_structure_get_string (str, "media");
  gboolean is_video = (!g_strcmp0 (media, "video"));

  /* Need to check if the pad created by the source is for video and not
   * audio. */
  if (g_strrstr (name, "x-rtp") && is_video) {

    GstPad *sinkpad = gst_element_get_static_pad (depay_elem, "sink");
    if (gst_pad_link (element_src_pad, sinkpad) != GST_PAD_LINK_OK) {
      g_printerr ("Failed to link depay loader to rtsp src");
    }
    gst_object_unref (sinkpad);
  }
  gst_caps_unref (caps);
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL, *source = NULL, *depayloader = NULL, *jpegparser = NULL, 
      *queue1 = NULL, *decoder = NULL, *videoconverter = NULL, *nvvidconv1 = NULL, *streammux = NULL, 
      *sink = NULL, *pgie = NULL, *nvvidconv2 = NULL, *nvosd = NULL;

  GstElement *transform = NULL;
  GstCaps *caps1 = NULL, *caps2 = NULL;
  GstElement *cap_filter1= NULL, *cap_filter2= NULL;
  GstBus *bus = NULL;
  guint bus_watch_id;
  GstPad *osd_sink_pad = NULL;

  int current_device = -1;
  cudaGetDevice(&current_device);
  struct cudaDeviceProp prop;
  cudaGetDeviceProperties(&prop, current_device);
  /* Check input arguments */
  if (argc != 2) {
    g_printerr ("Usage: %s <H264 filename>\n", argv[0]);
    return -1;
  }

  /* Standard GStreamer initialization */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  /* Create Pipeline element that will form a connection of other elements */
  pipeline = gst_pipeline_new ("dstest1-pipeline");

  /* Source element for reading from the file */
  source = gst_element_factory_make ("rtspsrc", "rtsp-source");

  g_object_set (G_OBJECT (source), "location", argv[1], NULL);
  

  /* Since the data format in the input file is elementary h264 stream,
   * we need a h264parser */
  depayloader = gst_element_factory_make("rtpjpegdepay", "depayloader");

  jpegparser = gst_element_factory_make ("jpegparse", "jpeg-parser");

  /*queue1 = gst_element_factory_make ("queue", "queue1");*/

  /* Use nvdec_h264 for hardware accelerated decode on GPU */

  cap_filter1 = gst_element_factory_make ("capsfilter", "cap_filter1");
  caps1 = gst_caps_from_string ("video/x-raw, format=NV12");
  g_object_set (G_OBJECT (cap_filter1), "caps1", caps1, NULL);
  gst_caps_unref (caps1);

  cap_filter2 = gst_element_factory_make ("capsfilter", "cap_filter2");
  caps2 = gst_caps_from_string ("video/x-raw(memory:NVMM), format=NV12");  
  g_object_set (G_OBJECT (cap_filter2), "caps", caps2, NULL);
  gst_caps_unref (caps2);

  decoder = gst_element_factory_make ("nvv4l2decoder", "nvv4l2-decoder");

  if(prop.integrated) {
    g_object_set (G_OBJECT (decoder), "mjpeg", 1, NULL);
  }

  videoconverter = gst_element_factory_make ("videoconvert", "video-converter");

  nvvidconv1 = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter1");

  if (!pipeline || !source || !depayloader || !jpegparser || !decoder || !cap_filter1 || !cap_filter2
      || !videoconverter || !nvvidconv1) {
    g_printerr ("One element in source end could not be created.\n");
    return -1;
  }
  

  /* Create nvstreammux instance to form batches from one or more sources. */
  streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");

  /* Use nvinfer to run inferencing on decoder's output,
   * behaviour of inferencing is set through config file */
  pgie = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");

  /* Use convertor to convert from NV12 to RGBA as required by nvosd */
  nvvidconv2 = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter2");

  /* Create OSD to draw on the converted RGBA buffer */
  nvosd = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");

  /* Finally render the osd output */
  if(prop.integrated) {
    transform = gst_element_factory_make ("nvegltransform", "nvegl-transform");
  }
  sink = gst_element_factory_make ("nveglglessink", "nvvideo-renderer");


  if (!streammux ||!pgie || !nvvidconv2 || !nvosd || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }
  
  if(!transform && prop.integrated) {
    g_printerr ("One tegra element could not be created. Exiting.\n");
    return -1;
  }
  g_object_set (G_OBJECT (sink), "sync", FALSE, NULL);

  g_object_set (G_OBJECT (streammux), "live-source", 1, NULL);

  g_object_set (G_OBJECT (streammux), "batch-size", 1, NULL);

  g_object_set (G_OBJECT (streammux), "width", MUXER_OUTPUT_WIDTH, "height",
      MUXER_OUTPUT_HEIGHT,
      "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, NULL);

  /* Set all the necessary properties of the nvinfer element,
   * the necessary ones are : */
  g_object_set (G_OBJECT (pgie),
      "config-file-path", "dstest1_pgie_config.txt", NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* Set up the pipeline */
  /* we add all elements into the pipeline */
  if(prop.integrated) {
    gst_bin_add_many (GST_BIN (pipeline), source, depayloader, jpegparser, decoder, videoconverter, cap_filter1, nvvidconv1, cap_filter2, streammux, pgie, nvvidconv2, nvosd, transform, sink, NULL);
  }
  else {
  gst_bin_add_many (GST_BIN (pipeline), source, depayloader, jpegparser, decoder, videoconverter, cap_filter1, nvvidconv1, cap_filter2, streammux, pgie, nvvidconv2, nvosd, sink, NULL);
  }

  GstPad *sinkpad, *srcpad;
  gchar pad_name_sink[16] = "sink_0";
  gchar pad_name_src[16] = "src";

  sinkpad = gst_element_get_request_pad (streammux, pad_name_sink);
  if (!sinkpad) {
    g_printerr ("Streammux request sink pad failed. Exiting.\n");
    return -1;
  }

  srcpad = gst_element_get_static_pad (cap_filter2, pad_name_src);
  if (!srcpad) {
    g_printerr ("Decoder request src pad failed. Exiting.\n");
    return -1;
  }

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_printerr ("Failed to link decoder to stream muxer. Exiting.\n");
      return -1;
  }

  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  /* we link the elements together */
  /* file-source -> h264-parser -> nvh264-decoder ->
   * nvinfer -> nvvidconv -> nvosd -> video-renderer */

  if (!gst_element_link_many (depayloader, jpegparser, decoder, videoconverter, cap_filter1, nvvidconv1, cap_filter2, NULL)) {
    g_printerr ("Elements could not be linked: 1. Exiting.\n");
    return -1;
  }
  g_signal_connect(source, "pad-added", G_CALLBACK(cb_newpad), depayloader);

  if(prop.integrated) {
    if (!gst_element_link_many (streammux, pgie, nvvidconv2, nvosd, transform, sink, NULL)) {
      g_printerr ("Elements could not be linked: 2. Exiting.\n");
      return -1;
    }
  }
  else {
    if (!gst_element_link_many (streammux, pgie, nvvidconv2, nvosd, sink, NULL)) {
      g_printerr ("Elements could not be linked: 2. Exiting.\n");
      return -1;
    }
  }

  /* Lets add probe to get informed of the meta data generated, we add probe to
   * the sink pad of the osd element, since by that time, the buffer would have
   * had got all the metadata. */
  osd_sink_pad = gst_element_get_static_pad (nvosd, "sink");
  if (!osd_sink_pad)
    g_print ("Unable to get sink pad\n");
  else
    gst_pad_add_probe (osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
        osd_sink_pad_buffer_probe, NULL, NULL);
  gst_object_unref (osd_sink_pad);

  /* Set the pipeline to "playing" state */
  g_print ("Now playing: %s\n", argv[1]);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait till pipeline encounters an error or EOS */
  g_print ("Running...\n");
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}
