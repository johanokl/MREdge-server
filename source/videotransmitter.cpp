/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "videotransmitter.h"
#include <QDebug>
#include <QThread>
#include <gst/app/app.h>
#include <gst/gstutils.h>
#include <mutex>
#include <QFile>
#include <QMetaEnum>
#include <QTimer>
#include <QTime>

#define WRITE_LOG_TRIGGER_TIME 63

namespace MREdge {

class GStreamerTransmitterContext {
public:
  GStreamerTransmitterContext(GstAppSrc *_appsrc) {
    initialized = false;
    sourceid = 0;
    appsrc = _appsrc;
    mainloop = nullptr;
    encoder = nullptr;
    if (_appsrc != nullptr) {
      mainloop = g_main_loop_new(nullptr, FALSE);
    }
  }
  GstAppSrc *appsrc;
  GMainLoop *mainloop;
  GstElement* encoder;
  uint sourceid;
  quint16 destPort;
  bool initialized;
  char _a, _b, _c;
  QSize videoSize;
  QMap<int, int> frames_processed_per_time_slice;
};




/**
 * @brief VideoTransmitter::VideoTransmitter
 * @param session Session id.
 * @param destHost Host to send the stream to.
 */
VideoTransmitter::VideoTransmitter(qint32 session, QString destHost)
{
  mTimer = nullptr;
  mDestPort = 0;
  mFormat = UNDEFINED;
  mSession = session;
  mDestHost = destHost;
  mBitrate = 5000;
  mStreamContext = new GStreamerTransmitterContext(nullptr);
  mGStreamerTransmitter = new GStreamerTransmitter();
  mGStreamerTransmitter->moveToThread(new QThread());
  mGStreamerTransmitter->thread()->start();
  mUptime = nullptr;
  mLogWritten = false;

  mWriteLogTimer = new QTimer(this);
  connect(mWriteLogTimer, &QTimer::timeout,
          [=]() {
    if (getBenchmarkingMode()) {
      writeLog();
    }
  });
  mWriteLogTimer->start(WRITE_LOG_TRIGGER_TIME * 1000);

  QObject::connect(this, &VideoTransmitter::startTransmitter,
                   mGStreamerTransmitter, &GStreamerTransmitter::start);
  QObject::connect(mGStreamerTransmitter, &GStreamerTransmitter::started,
                   [=](GStreamerTransmitterContext *ctx) { mStreamContext = ctx; });
  init();
}

/**
 * @brief VideoTransmitter::~VideoTransmitter
 */
VideoTransmitter::~VideoTransmitter()
{
  mWriteLogTimer->stop();
  delete mWriteLogTimer;
  if (getBenchmarkingMode()) {
    writeLog();
  }
  if (mStreamContext && mStreamContext->mainloop != nullptr && g_main_loop_is_running(mStreamContext->mainloop)) {
    g_main_loop_quit(mStreamContext->mainloop);
  }
}

/**
 * @brief VideoTransmitter::writeLog
 */
void VideoTransmitter::writeLog()
{
  if (mLogWritten) {
    return;
  }
  mLogWritten = true;
  QString formatString;
  if (mFormat == VideoStreamer::Format::H264_UDP) {
    formatString = "H264_UDP";
  } else if (mFormat == VideoStreamer::Format::H264_TCP) {
    formatString = "H264_TCP";
  }
  QString filename = QString("transmitter_fps_%1_%2.csv")
      .arg(formatString).arg(QTime::currentTime().toString());
  filename.replace(QString(":"), QString(""));

  QFile file(filename);
  if (file.open(QIODevice::WriteOnly | QIODevice::Append | QFile::Text)) {
    file.seek(file.size());
    QTextStream out(&file);
    for (int i = 0; i < WRITE_LOG_TRIGGER_TIME; i++) {
      out << i;
      out << ", ";
      out << mStreamContext->frames_processed_per_time_slice.value(i, 0);
      out << "\n";
    }
    file.close();
  }
}

/**
 * @brief VideoTransmitter::startStream
 * @param format Stream format to use.
 * @param destPort Port to use at the destination server.
 */
void VideoTransmitter::startStream(Format format, quint16 destPort)
{
  if (mDestPort != destPort || mFormat != format) {
    mFormat = format;
    mDestPort = destPort;
    init();
  }
}

/**
 * @brief VideoTransmitter::setImageSize
 * @param size Video image resolution width and height.
 */
void VideoTransmitter::setImageSize(QSize size)
{
  if (mImageSize != size) {
    mImageSize = size;
    init();
  }
}

/**
 * @brief VideoTransmitter::setBitrate
 * @param bitrate Bitrate in for output stream in kbit/s.
 */
void VideoTransmitter::setBitrate(int bitrate)
{
  mBitrate = bitrate;
  if (mStreamContext && mStreamContext->encoder) {
    g_object_set(
          reinterpret_cast<GObject *>(mStreamContext->encoder),
          "bitrate", bitrate,
          nullptr);
  }
}

/**
 * @brief VideoTransmitter::init
 */
void VideoTransmitter::init()
{
  if (mDestPort == 0 || mImageSize.isEmpty()) {
    return;
  }
  if (mStreamContext != nullptr) {
    if (mStreamContext->videoSize == mImageSize &&
        mStreamContext->destPort == mDestPort) {
      return;
    }
    mStreamContext->initialized = false;
    if (mStreamContext->mainloop != nullptr) {
      g_main_loop_quit(mStreamContext->mainloop);
    }
  }
  emit startTransmitter(mDestHost, mDestPort, mFormat, mImageSize, mBitrate);
}

/**
 * @brief VideoTransmitter::addQImageToProcessQueue
 * @param session Session id.
 * @param image New video frame to add to the streamer's buffer.
 */
void VideoTransmitter::addQImageToProcessQueue(qint32 session, quint32 frameid, QImage image)
{
  if (!mStreamContext || !mStreamContext->initialized || session != mSession) {
    return;
  }
  Q_UNUSED(frameid);
  mMutex.lock();
  if (mValidImage) {
    fDebug << "addQImageToProcessQueue: Buffer not empty. Replace image.";
  }
  mImage = image;
  mValidImage = true;
  mMutex.unlock();
}

/**
 * @brief VideoTransmitter::processQImage
 * @param session Session id.
 * @param image New video frame to encode and transmit.
 *
 * Sends the image to GStreamer's inbox, to be encoded and transmitted using
 * the configured streaming format.
 */
void VideoTransmitter::processQImage(qint32 session, quint32 frameid, QImage image)
{
  if (!mStreamContext || !mStreamContext->initialized || session != mSession) {
    return;
  }
  Q_UNUSED(frameid);
  bool isValid = false;
  mMutex.lock();
  image = mImage;
  isValid = mValidImage;
  mValidImage = false;
  mMutex.unlock();
  if (!isValid) {
    fDebug << "Frame skipped. Inflow faster than they could be processed.";
    return;
  }
  if (mStreamContext->videoSize != image.size()) {
    fDebug << QString("Image is wrong size. Should be (%1, %2)")
              .arg(mStreamContext->videoSize.width())
              .arg(mStreamContext->videoSize.height());
    return;
  }
  if (mTimer == nullptr) {
    mTimer = new QElapsedTimer;
    mTimer->start();
  }
  auto buffer = gst_buffer_new_and_alloc(static_cast<gsize>(image.byteCount()));
  gst_buffer_fill(buffer, 0, image.constBits(), static_cast<gsize>(image.byteCount()));
  gst_buffer_set_flags(buffer, GST_BUFFER_FLAG_LIVE);
  buffer->pts = static_cast<GstClockTime>(mTimer->nsecsElapsed());
  auto result = gst_app_src_push_buffer(mStreamContext->appsrc, buffer);

  auto time_slice = mTimer->elapsed() / 1000;
  mStreamContext->frames_processed_per_time_slice[time_slice] += 1;
  if (mLogTime && mUptime) {
    mFramesProcessedNsec.insert(frameid, mUptime->nsecsElapsed());
  }
  if (result != GST_FLOW_OK) {
    fDebug << "Failure when pushing image" << frameid;
  }
}

/**
 * @brief VideoTransmitter::getProcessingTimes
 * @return
 */
QMap<quint32, qint64> VideoTransmitter::getProcessingTimes() const
{
  auto retMap = mFramesProcessedNsec;
  retMap.detach();
  return retMap;
}

/**
 * @brief gstreamer_error_callback
 * @param msg
 * @param data
 */
static void gstreamer_error_callback(GstBus *, GstMessage *msg, GStreamerTransmitterContext *data)
{
  Q_UNUSED(data);
  GError *err;
  gchar *debug_info;
  gst_message_parse_error(msg, &err, &debug_info);
  fDebug << QString("Error received from element: %1, %2")
            .arg(msg->src->name, err->message);
  fDebug << "Debugging information: " << (debug_info ? debug_info : "none");
  g_clear_error(&err);
  g_free(debug_info);
}

/**
 * @brief gstreamer_qos_callback
 * @param msg
 * @param data
 */
static void gstreamer_qos_callback(GstBus *, GstMessage *msg, GStreamerTransmitterContext *data)
{
  Q_UNUSED(data);
  GError *err;
  gchar *debug_info;
  gst_message_parse_error(msg, &err, &debug_info);
  fDebug << QString("QOS received from element: %1, %2")
            .arg(msg->src->name, err->message);
  fDebug << "Debugging information: " << (debug_info ? debug_info : "none");
  g_clear_error(&err);
  g_free(debug_info);
}

/**
 * @brief GStreamerTransmitter::start
 * @param destHost Destination host.
 * @param destPort Destination port.
 * @param format Streamer format.
 * @param size Video frame resolution.
 */
void GStreamerTransmitter::start(QString destHost, quint16 destPort,
                                 VideoStreamer::Format format,
                                 QSize size, int bitrate)
{
  gst_init(nullptr, nullptr);

  fDebug << QString("start: destination host=%1, port=%2, size=(%3, %4), video=(%5, bitrate=%6)")
            .arg(destHost).arg(destPort).arg(size.width()).arg(size.height())
            .arg(format).arg(bitrate);

  auto appsrc = gst_element_factory_make("appsrc", "appsrc");
  auto videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
  auto x264enc = gst_element_factory_make("x264enc", "x264enc");
  auto tcpclientsink = gst_element_factory_make("tcpclientsink", "tcpclientsink");
  auto rtph264pay = gst_element_factory_make("rtph264pay", "rtph264pay");
  auto udpsink = gst_element_factory_make("udpsink", "udpsink");
  auto pipeline = gst_pipeline_new("pipeline");
  auto mpegtsmux = gst_element_factory_make("mpegtsmux", "mpegtsmux");

  g_object_set
      (reinterpret_cast<GObject *>(appsrc),
       "stream-type", 0,
       "is-live", TRUE,
       "do-timestamp", TRUE,
       "format", GST_FORMAT_TIME,
       "caps", gst_caps_new_simple(
         "video/x-raw",
         "format", G_TYPE_STRING, "RGB",
         "width", G_TYPE_INT, size.width(),
         "height", G_TYPE_INT, size.height(),
         "framerate", GST_TYPE_FRACTION, 30, 1,
         nullptr),
       nullptr);

  g_object_set(
        reinterpret_cast<GObject *>(x264enc),
        "speed-preset", 1, // ultrafast
        "bitrate", bitrate, // Bitrate in kbit/sec. [1,2048000] Default: 2048
        //"rc-lookahead", 1,
        "tune", 4, // zerolatency
        //"insert-vui", TRUE,
        nullptr);

  if (format == VideoStreamer::Format::H264_UDP) {
    g_object_set(reinterpret_cast<GObject *>(x264enc),
                 "key-int-max", 1, nullptr);
  }
  g_object_set(
        reinterpret_cast<GObject *>(rtph264pay),
        "config-interval", 1,
        "pt", 96,
        nullptr);

  destHost.replace("::ffff:", "", Qt::CaseInsensitive);

  g_object_set(
        reinterpret_cast<GObject *>(udpsink),
        "host", destHost.toLocal8Bit().constData(),
        "port", destPort,
        "async", TRUE,
        nullptr);

  g_object_set(
        reinterpret_cast<GObject *>(tcpclientsink),
        "host", destHost.toLocal8Bit().constData(),
        "port", destPort,
        "async", TRUE,
        nullptr);

  gboolean result = 0;

  switch (format) {
  case VideoStreamer::Format::H264_UDP:
    fDebug << "H264_UDP";
    gst_bin_add_many(
          reinterpret_cast<GstBin *>(pipeline),
          appsrc, videoconvert, x264enc, rtph264pay, udpsink, nullptr);
    result = gst_element_link_many(
          appsrc,
          videoconvert,
          x264enc,
          rtph264pay,
          udpsink,
          nullptr);
    break;
  case VideoStreamer::Format::H264_TCP:
    fDebug << "H264_TCP";
    gst_bin_add_many(
          reinterpret_cast<GstBin *>(pipeline),
          appsrc, videoconvert, x264enc, mpegtsmux, tcpclientsink, nullptr);
    result = gst_element_link_many(
          appsrc,
          videoconvert,
          x264enc,
          mpegtsmux,
          tcpclientsink,
          nullptr);
    break;
  default:
    fDebug << "Format not set";
    return;
  }

  if (!result) {
    fDebug << "Unable to initialize GST";
    return;
  }

  auto ctx = new GStreamerTransmitterContext(
        reinterpret_cast<GstAppSrc *>(appsrc));

  ctx->destPort = destPort;
  ctx->videoSize = size;

  auto status = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  fDebug << "Status:" << status;
  ctx->initialized = true;

  auto bus = gst_pipeline_get_bus(reinterpret_cast<GstPipeline *>(pipeline));
  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message::error", G_CALLBACK(gstreamer_error_callback), ctx);
  g_signal_connect(bus, "message::qos", G_CALLBACK(gstreamer_qos_callback), ctx);
  gst_object_unref(bus);

  emit started(ctx);

  g_main_loop_run(ctx->mainloop);
  fDebug << "finished transmitter";
  gst_element_set_state(pipeline, GST_STATE_NULL);
  if (ctx->mainloop != nullptr) {
    g_free(ctx->mainloop);
  }
  ctx->mainloop = nullptr;
  g_free(ctx);
}

}
