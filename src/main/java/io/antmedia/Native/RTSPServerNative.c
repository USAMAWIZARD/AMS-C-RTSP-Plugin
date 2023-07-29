#define PIPE_GSTREAMER "Gstreamer"
#define PIPE_FFMPEG "FFmpeg"
#define PIPE_RTSP "RTSP"

#include <stdio.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>


static char *port = "8554";
GstRTSPMountPoints *mounts;
GHashTable *hash_table;

typedef struct
{
  GstAppSrc *appsrc;
  GstElement *pipeline;
  volatile gboolean pipeline_initialized;
} StreamMap;


int add_rtsp_pipeline(gchar *streamId);
void sendPacket(AVPacket *pktPointer, gchar *streamId);
void init_rtsp_server();
void register_pipeline(gchar *streamId, gchar *pipeline_type, gchar *pipeline);
int add_gstreamer_pipeline(char *pipeline, char *streamId);
int add_rtsp_pipeline(gchar *streamId);


void check_err(int exp, char *msg, int is_exit)
{
  if (!exp)
  {
    perror(msg);
    if (is_exit)
    {
      exit(EXIT_FAILURE);
    }
  }
}
void sig_handler()
{
}

void sendPacket(AVPacket *pktPointer, gchar *streamId)
{
  AVPacket *pkt = (AVPacket *)pktPointer;
  // printf("Packet PTS %s: %ld %ld\n ", id, pkt->pts, pkt->dts);

  if (g_hash_table_contains(hash_table, streamId))
  {
    StreamMap *ctx = (StreamMap *)g_hash_table_lookup(hash_table, streamId);

    if (ctx->pipeline_initialized)
    {
      GstBuffer *buffer = gst_buffer_new_and_alloc(pkt->size);
      uint8_t *data = (uint8_t *)pkt->data;
      gst_buffer_fill(buffer, 0, data, pkt->size);

      g_assert(ctx->appsrc);

      if ((pkt->flags & AV_PKT_FLAG_KEY))
      {
        printf("-----------------key--frame--------------------- \n");
      }
      else
      {
        GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      }
      gst_app_src_push_buffer((GstAppSrc *)ctx->appsrc, buffer);
    }
  }
  else
  {
  }
}
void register_pipeline(gchar *streamId, gchar *pipeline_type, gchar *pipeline)
{
  // register appropriate pipline
  if (g_hash_table_contains(hash_table, streamId))
  {
    if(g_strcmp0(pipeline_type,PIPE_GSTREAMER)==0){
      
    }
    else if(g_strcmp0(pipeline_type,PIPE_FFMPEG)==0){

    }
    else if(g_strcmp0(pipeline_type,PIPE_RTSP)==0){

    }
    else{
      //invalid option
    }
  }
  else
  {
    //sreamid not found
  }
}

static void media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer streamid)
{

  if (g_hash_table_contains(hash_table, streamid))
  {
    g_print("------------------------");
    GstElement *element, *appsrc;
    element = gst_rtsp_media_get_element(media);

    appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), (gchar *)streamid);

    StreamMap *ctx = (StreamMap *)g_hash_table_lookup(hash_table, (gchar *)streamid);
    ctx->appsrc = (GstAppSrc *)appsrc;
    ctx->pipeline_initialized = 1;
    ctx->pipeline = element;

    gst_object_unref(element);
  }
  else
  {
    return;
  }
}

int add_rtsp_pipeline(gchar *streamId)
{
  GstRTSPMediaFactory *factory;
  gchar pipe[700];
  gchar mountpoint[30];
  factory = gst_rtsp_media_factory_new();
  sprintf(pipe, "appsrc name=%s is-live=true  do-timestamp=true ! queue ! capsfilter caps=\"video/x-h264, alignment=(string)nal, stream-format=(string)byte-stream,profile=baseline \"  !  h264parse ! rtph264pay name=pay0 pt=96", streamId);

  g_signal_connect(factory, "media-configure", (GCallback)media_configure, (gpointer)streamId);

  gst_rtsp_media_factory_set_launch(factory, pipe);

  sprintf(mountpoint, "/%s", streamId);
  gst_rtsp_mount_points_add_factory(mounts, mountpoint, factory);
  g_print("New stream ready at rtsp://127.0.0.1:%s/%s\n", port, streamId);
}

int add_gstreamer_pipeline(char *pipeline, char *streamId)
{
  gchar pipe[700];
  gchar mountpoint[30];
  StreamMap *ctx;
  if (g_hash_table_contains(hash_table, streamId))
  {
    ctx = (StreamMap *)g_hash_table_lookup(hash_table, streamId);
    sprintf(pipe, "appsrc  name=%s is-live=true  do-timestamp=true ! queue ! capsfilter caps=\"video/x-h264, alignment=(string)nal, stream-format=(string)byte-stream,profile=baseline \" !  h264parse ! %s", streamId, pipeline);
    ctx->pipeline = gst_parse_launch(pipe, NULL);
    gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
    ctx->pipeline_initialized = 1;
    return 0;
  }
  else
  {
    return -1;
  }
}
int add_ffmpeg_pipeline(char *pipeline, char *streamId){

  return 0;
}


void register_stream(char *streamId)
{
  printf("new streamid recived adding new stream : %s \n", streamId);
  StreamMap *ctx = g_new0(StreamMap, 1);
  ctx->pipeline_initialized = 0;
  g_hash_table_insert(hash_table, streamId, ctx);

  add_rtsp_pipeline(streamId);

  printf("added stream id\n");
}
void unregister_stream(char *streamId)
{
  if (g_hash_table_contains(hash_table, streamId))
  {
    StreamMap *ctx = (StreamMap *)g_hash_table_lookup(hash_table, streamId);
    gst_element_set_state((GstElement *)ctx->appsrc, GST_STATE_NULL);
    gst_element_set_state(ctx->pipeline, GST_STATE_NULL);            
    g_hash_table_remove(hash_table, streamId);
    gst_object_unref(ctx->pipeline);
    gst_object_unref(ctx->appsrc);
    printf("stream unregistered\n");
  }
}
void init_rtsp_server()
{
  GMainLoop *loop;
  GstRTSPServer *server;
  GstRTSPMediaFactory *factory;
  GError *error = NULL;
  hash_table = g_hash_table_new(g_str_hash, g_str_equal);
  
  setenv("GST_DEBUG", "3", 1);
  setenv("GST_DEBUG_FILE", "./loggggggggggggggggggggggg.log", 1);

  gst_init(NULL, NULL);
  g_print("-----------------------------------------------%s /n", getenv("GST_DEBUG"));

  loop = g_main_loop_new(NULL, FALSE);
  server = gst_rtsp_server_new();
  check_err(server != NULL, "nullllllllllllllllllllllllllllllllllllllllllllllllllllll", 1);

  g_object_set(server, "service", port, NULL);
  printf("initialized RTSP Server Listening on Port %s \n", port);
  mounts = gst_rtsp_server_get_mount_points(server);
  // add_rtsp_pipeline("Streamid");

  g_object_unref(mounts);

  gst_rtsp_server_attach(server, NULL);

  g_main_loop_run(loop);
}