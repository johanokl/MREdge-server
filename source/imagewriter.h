/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef IMAGEWRITER_H
#define IMAGEWRITER_H

#include "global.h"
#include "networkconnection.h"
#include <QHostAddress>
#include <QObject>

namespace MREdge {

class ImageWriter : public QObject {
  Q_OBJECT

public:
  ImageWriter(QString path) : mPath(path) {}
  ~ImageWriter() {}

public slots:
  void writeImage(qint32 session, NetworkConnection::File file);
  void writeMat(qint32 session, quint32 frameid, cvMatPtr image);
  void setEnabled(bool enabled) {
    mEnabled = enabled;
  }

private:
  bool mEnabled = true;
  QString mPath;

};

}

#endif // IMAGEWRITER_H
