/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include <gst/app/app.h>
#include "videoreceiver.h"
#include <QDebug>
#include <QObject>
#include <QThread>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDataStream>
#include <QNetworkInterface>
#include <QDebug>
#include <QFile>
#include <vector>
#include <stdio.h>
#include <QTimer>
#include <QMetaEnum>
#include <opencv2/opencv.hpp>

#define WRITE_LOG_TRIGGER_TIME 63

namespace MREdge {


class GStreamerReceiverContext {
public:
  GStreamerReceiverContext(GStreamerReceiver *_parent) {
    parent = _parent;
    reachedEndOfStream = false;
    running = false;
    timer = nullptr;
    frames_sent = 0;
  }
  // Share frame between main loop and gstreamer callback
  cvMatPtr latestFrame;
  bool reachedEndOfStream;
  bool running;
  QElapsedTimer *timer;
  QMap<int, int> frames_processed_per_time_slice;
  GMainLoop *mainLoop;
  GStreamerReceiver *parent;
  quint32 frames_sent;
};

/**
 * @brief VideoReceiver::VideoReceiver
 * @param session Session id
 * @param senderHost The host name of the stream sender.
 */
VideoReceiver::VideoReceiver(qint32 session, QString senderHost)
{
  mSession = session;
  mSenderHost = senderHost;
  mPort = 0;
  mFormat = UNDEFINED;
  mGStreamerReceiver = new GStreamerReceiver(this);
  mGStreamerReceiver->moveToThread(new QThread());
  mGStreamerReceiver->thread()->start();

  mWriteLogTimer = new QTimer(this);
  connect(mWriteLogTimer, &QTimer::timeout,
          [=]() {
    if (getBenchmarkingMode()) {
      mGStreamerReceiver->writeLog();
    }
  });
  mWriteLogTimer->start(WRITE_LOG_TRIGGER_TIME * 1000);

  QObject::connect(mGStreamerReceiver, &GStreamerReceiver::ready,
                   [=](VideoStreamer::Format format, quint16 port) {
    mPort = port;
    mFormat = format;
    emit ready(mSession, format, port);
  });
  QObject::connect(this, &VideoReceiver::startReceiver,
                   mGStreamerReceiver, &GStreamerReceiver::start);
}

/**
 * @brief VideoTransmitter::getProcessingTimes
 * @return
 */
QMap<quint32, qint64> VideoReceiver::getProcessingTimes() const
{
  auto retMap = mFramesArrivedNsec;
  retMap.detach();
  return retMap;
}

/**
 * @brief VideoReceiver::newMat
 * @param image
 * @param frameid
 */
void VideoReceiver::newMat(quint32 frameid, cvMatPtr image)
{
  if (mLogTime && mUptime) {
    mFramesArrivedNsec.insert(frameid, mUptime->nsecsElapsed());
  }
  emit matReady(mSession, frameid, image);
}

/**
 * @brief VideoReceiver::~VideoReceiver
 */
VideoReceiver::~VideoReceiver()
{
  if (mGStreamerReceiver) {
    mGStreamerReceiver->stop();
  }
  mWriteLogTimer->stop();
  delete mWriteLogTimer;
  if (getBenchmarkingMode()) {
    mGStreamerReceiver->writeLog();
  }
}

/**
 * @brief VideoReceiver::start
 * @param format Stream format to start.
 * Starts the receiver in its thread.
 */
void VideoReceiver::start(Format format, bool userJitterbuffer)
{
  if (mFormat != format) {
    mFormat = format;
    mGStreamerReceiver->stop();
    emit startReceiver(format, mSenderHost, userJitterbuffer);
  }
}

/**
 * @brief new_preroll_callback
 * Unused GStreamer callback function.
 */
GstFlowReturn new_preroll_callback(GstAppSink * /*appsink*/, gpointer /*ctx*/)
{
  fDebug << "new_preroll";
  return GST_FLOW_OK;
}

/**
 * @brief eos_stream_callback
 * @return
 */
void eos_stream_callback(GstAppSink * /*appsink*/, gpointer /*ctx*/)
{
  fDebug << "eos_stream";
}

/**
 * @brief frame_received_callback
 * @param appsink Active GStreamer appsink
 * @param data The GStreamer context.
 *
 * GStreamer has received a new frame (image). Copy it into the
 * appropriate receiver's context so that it can be sent to the
 * listeners.
 */
GstFlowReturn frame_received_callback(GstAppSink *appsink, gpointer data)
{
  auto ctx = reinterpret_cast<GStreamerReceiverContext *>(data);
  if (!ctx) {
    fDebug << "Error: frame_received_callback could not convert data.";
    return GST_FLOW_ERROR;
  }
  // Get caps and frame
  auto sample = gst_app_sink_pull_sample(appsink);
  auto caps = gst_sample_get_caps(sample);
  auto buffer = gst_sample_get_buffer(sample);
  auto structure = gst_caps_get_structure(caps, 0);
  int width = g_value_get_int(gst_structure_get_value(structure, "width"));
  int height = g_value_get_int(gst_structure_get_value(structure, "height"));
  // Get frame data
  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);
  // Convert gstreamer data to OpenCV Mat
  auto currFrame = cvMatPtr(new cv::Mat(cv::Size(width, height), CV_8UC3));
  memcpy(currFrame->data, map.data, width * height * 3);

  gst_buffer_unmap(buffer, &map);
  gst_sample_unref(sample);
  ctx->frames_sent++;
  if (ctx->timer == nullptr) {
    ctx->timer = new QElapsedTimer;
    ctx->timer->start();
  }
  auto time_slice = ctx->timer->elapsed() / 1000;
  ctx->frames_processed_per_time_slice[time_slice] += 1;
  ctx->parent->parent()->newMat(ctx->frames_sent, currFrame);
  return GST_FLOW_OK;
}

/**
 * @brief gst_bus_callbacks
 * GStreamer callback
 */
static gboolean gst_bus_callbacks(GstBus*, GstMessage *message, gpointer data)
{
  fDebug << "gst_bus_callbacks";
  auto ctx = reinterpret_cast<GStreamerReceiverContext *>(data);
  if (!ctx) {
    fDebug << "Error: gst_bus_callbacks could not convert data.";
    return true;
  }
  switch (static_cast<GstMessage *>(message)->type) {
  case GST_MESSAGE_ERROR: {
    GError *err;
    gchar *debug;
    gst_message_parse_error(message, &err, &debug);
    fDebug << "Error: " << err->message;
    g_error_free(err);
    g_free(debug);
    break;
  }
  case GST_MESSAGE_WARNING: {
    GError *err;
    gchar *debug;
    gst_message_parse_warning(message, &err, &debug);
    fDebug << "Warning: " << err->message;
    g_error_free(err);
    g_free(debug);
    break;
  }
  case GST_MESSAGE_INFO: {
    GError *err;
    gchar *debug;
    gst_message_parse_info(message, &err, &debug);
    fDebug << "Info: " << err->message;
    g_error_free(err);
    g_free(debug);
    break;
  }
  case GST_MESSAGE_EOS:
    ctx->reachedEndOfStream = true;
    fDebug << "End of stream";
    break;
  case GST_MESSAGE_UNKNOWN:
    fDebug << "GST_MESSAGE_UNKNOWN";
    break;
  case GST_MESSAGE_TAG:
    fDebug << "GST_MESSAGE_TAG";
    break;
  case GST_MESSAGE_BUFFERING:
    fDebug << "GST_MESSAGE_BUFFERING";
    break;
  case GST_MESSAGE_STATE_CHANGED:
    fDebug << "GST_MESSAGE_STATE_CHANGED";
    break;
  case GST_MESSAGE_STATE_DIRTY:
    fDebug << "GST_MESSAGE_STATE_DIRTY";
    break;
  case GST_MESSAGE_STEP_DONE:
    fDebug << "GST_MESSAGE_STEP_DONE";
    break;
  case GST_MESSAGE_CLOCK_PROVIDE:
    fDebug << "GST_MESSAGE_CLOCK_PROVIDE";
    break;
  case GST_MESSAGE_CLOCK_LOST:
    fDebug << "GST_MESSAGE_CLOCK_LOST";
    break;
  case GST_MESSAGE_NEW_CLOCK:
    fDebug << "GST_MESSAGE_NEW_CLOCK";
    break;
  case GST_MESSAGE_STRUCTURE_CHANGE:
    fDebug << "GST_MESSAGE_STRUCTURE_CHANGE";
    break;
  case GST_MESSAGE_STREAM_STATUS:
    fDebug << "GST_MESSAGE_STREAM_STATUS";
    break;
  case GST_MESSAGE_APPLICATION:
    fDebug << "GST_MESSAGE_APPLICATION";
    break;
  case GST_MESSAGE_ELEMENT:
    fDebug << "GST_MESSAGE_ELEMENT";
    break;
  case GST_MESSAGE_SEGMENT_START:
    fDebug << "GST_MESSAGE_SEGMENT_START";
    break;
  case GST_MESSAGE_SEGMENT_DONE:
    fDebug << "GST_MESSAGE_SEGMENT_DONE";
    break;
  case GST_MESSAGE_DURATION_CHANGED:
    fDebug << "GST_MESSAGE_DURATION_CHANGED";
    break;
  case GST_MESSAGE_LATENCY:
    fDebug << "GST_MESSAGE_LATENCY";
    break;
  case GST_MESSAGE_ASYNC_START:
    fDebug << "GST_MESSAGE_ASYNC_START";
    break;
  case GST_MESSAGE_ASYNC_DONE:
    fDebug << "GST_MESSAGE_ASYNC_DONE";
    break;
  case GST_MESSAGE_REQUEST_STATE:
    fDebug << "GST_MESSAGE_REQUEST_STATE";
    break;
  case GST_MESSAGE_STEP_START:
    fDebug << "GST_MESSAGE_STEP_START";
    break;
  case GST_MESSAGE_QOS:
    fDebug << "GST_MESSAGE_QOS";
    break;
  case GST_MESSAGE_PROGRESS:
    fDebug << "GST_MESSAGE_PROGRESS";
    break;
  case GST_MESSAGE_TOC:
    fDebug << "GST_MESSAGE_TOC";
    break;
  case GST_MESSAGE_RESET_TIME:
    fDebug << "GST_MESSAGE_RESET_TIME";
    break;
  case GST_MESSAGE_STREAM_START:
    fDebug << "GST_MESSAGE_STREAM_START";
    break;
  case GST_MESSAGE_NEED_CONTEXT:
    fDebug << "GST_MESSAGE_NEED_CONTEXT";
    break;
  case GST_MESSAGE_HAVE_CONTEXT:
    fDebug << "GST_MESSAGE_HAVE_CONTEXT";
    break;
  case GST_MESSAGE_REDIRECT:
    fDebug << "GST_MESSAGE_REDIRECT";
    break;
  case GST_MESSAGE_ANY:
    fDebug << "GST_MESSAGE_ANY";
    break;
  default:
    fDebug << "Message type unknown";
    break;
  }
  return true;
}

/**
 * @brief GStreamerReceiver::GStreamerReceiver
 */
GStreamerReceiver::GStreamerReceiver(VideoReceiver *parent)
{
  mParent = parent;
  mCtx = new GStreamerReceiverContext(this);
  mLogWritten = false;
}

/**
 * @brief GStreamerReceiver::~GStreamerReceiver
 */
GStreamerReceiver::~GStreamerReceiver()
{
  delete mCtx;
}

/**
 * @brief GStreamerReceiver::writeLog
 */
void GStreamerReceiver::writeLog()
{
  if (mLogWritten) {
    return;
  }
  mLogWritten = true;
  QString formatString;
  auto format = parent()->getFormat();
  if (format == VideoStreamer::Format::H264_UDP) {
    formatString = "H264_UDP";
  } else if (format == VideoStreamer::Format::H264_TCP) {
    formatString = "H264_TCP";
  }
  QString filename = QString("receiver_fps_%1_%2.csv")
      .arg(formatString).arg(QTime::currentTime().toString());
  filename.replace(QString(":"), QString(""));

  QFile file(filename);
  if (file.open(QIODevice::WriteOnly | QIODevice::Append | QFile::Text)) {
    file.seek(file.size());
    QTextStream out(&file);
    for (int i = 0; i < WRITE_LOG_TRIGGER_TIME; i++) {
      out << i;
      out << ", ";
      out << mCtx->frames_processed_per_time_slice.value(i, 0);
      out << "\n";
    }
    file.close();
  }
}

/**
 * @brief GStreamerReceiver::start
 * @param format Stream format to receive.
 * @param host Host to receive camera stream from.
 *
 * Starts the stream. This function will not return until the receiver has stopped,
 * so call it from another thread using a slot. When the receiver has started it will
 * emit the signal ready() with the port number the client can connect to.
 */
void GStreamerReceiver::start(VideoStreamer::Format format, QString destHost, bool jitterbuffer)
{
  gst_init(nullptr, nullptr);
  destHost.replace("::ffff:", "", Qt::CaseInsensitive);
  mCtx->running = true;
  mCtx->reachedEndOfStream = false;
  QString pipelineCmd;

  QString thisHost;

  if (format == VideoStreamer::Format::H264_TCP) {
    foreach (const auto &netInterface, QNetworkInterface::allInterfaces()) {
      QNetworkInterface::InterfaceFlags flags = netInterface.flags();
      if (thisHost.isEmpty() &&
          (flags & QNetworkInterface::IsRunning) &&
          !(flags & QNetworkInterface::IsLoopBack)) {
        foreach (const auto &address, netInterface.addressEntries()) {
          if (address.ip().protocol() == QAbstractSocket::IPv4Protocol) {
            thisHost = address.ip().toString().toLatin1().data();
            break;
          }
        }
      }
    }
  }

  switch (format) {
  case VideoStreamer::Format::H264_UDP:
    pipelineCmd = QString(
          "udpsrc name=insrc port=0 ! "
          "application/x-rtp,payload=96,encoding-name=H264 ! "
          "%1 "
          "rtph264depay ! "
          "h264parse ! "
          "avdec_h264 output-corrupt=false ! "
          "videoconvert ! "
          "video/x-raw,format=(string)RGB ! "
          "videoconvert ! "
          "appsink name=sink emit-signals=true "
          "max-buffers=1 drop=true"
          ).arg(jitterbuffer ? "rtpjitterbuffer !" : "");
    break;
  case VideoStreamer::Format::H264_TCP:
    pipelineCmd = QString(
          "tcpserversrc name=insrc port=0 host=%1 ! "
          "tsdemux ! "
          "h264parse ! "
          "avdec_h264 output-corrupt=false ! "
          "videoconvert ! "
          "video/x-raw,format=(string)RGB ! "
          "videoconvert ! "
          "appsink name=sink emit-signals=true "
          "max-buffers=1 drop=true"
          ).arg(thisHost);
    break;
  default:
    fDebug << "Format not set";
    return;
  }

  fDebug << pipelineCmd.toLocal8Bit().constData();

  GError *error = nullptr;
  auto pipeline = gst_parse_launch(pipelineCmd.toLocal8Bit().constData(), &error);

  if (error) {
    g_print("could not construct pipeline: %s\n", error->message);
    g_error_free(error);
    return;
  }

  auto insrc = gst_bin_get_by_name(reinterpret_cast<GstBin *>(pipeline), "insrc");
  auto sink = gst_bin_get_by_name(reinterpret_cast<GstBin *>(pipeline), "sink");
  gst_app_sink_set_emit_signals(reinterpret_cast<GstAppSink *>(sink), true);
  GstAppSinkCallbacks callbacks = {
    eos_stream_callback, new_preroll_callback, frame_received_callback, {nullptr}};
  gst_app_sink_set_callbacks(reinterpret_cast<GstAppSink *>(sink), &callbacks,
                             mCtx, nullptr);

  // Declare bus
  auto bus = gst_pipeline_get_bus(reinterpret_cast<GstPipeline *>(pipeline));
  gst_bus_add_watch(bus, gst_bus_callbacks, mCtx);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_PAUSED);
  gint port;
  g_object_get(insrc, "port", &port, nullptr);
  if (!port) {
    g_object_get(insrc, "current-port", &port, nullptr);
  }
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  emit ready(format, static_cast<quint16>(port));

  fDebug << "Port: " << port;

  mCtx->mainLoop = g_main_loop_new(nullptr, 0);
  g_main_loop_run(mCtx->mainLoop);

  fDebug << "Loop finished";

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(insrc);
  gst_object_unref(sink);
  gst_object_unref(pipeline);
}

/**
 * @brief GStreamerReceiver::stop
 */
void GStreamerReceiver::stop()
{
  if (mCtx->running) {
    fDebug << "Stopping GStreamerReceiver";
    mCtx->running = false;
    if (mCtx->mainLoop != nullptr) {
      g_main_quit(mCtx->mainLoop);
    }
    mCtx->mainLoop = nullptr;
  }
}


}
