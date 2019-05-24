#ifndef VIDEO_H
#define VIDEO_H

#include <QObject>
#include "networkconnection.h"

namespace MREdge {

class VideoStreamer : public NetworkConnection {
public:
  enum Format {
    UNDEFINED,
    H264_UDP,
    H264_TCP
  };
  Q_ENUM(Format)
  virtual ~VideoStreamer() {}
  void setBenchmarkingMode(bool enabled) { mBenchmarking = enabled;}
  bool getBenchmarkingMode() { return mBenchmarking; }
private:
  bool mBenchmarking = false;
};

}

#endif // VIDEO_H
