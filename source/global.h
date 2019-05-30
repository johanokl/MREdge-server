/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#ifndef GLOBAL_H
#define GLOBAL_H

#include <QByteArray>
#include <QImage>
#include <QSharedPointer>
#include <opencv/cxcore.hpp>
#include <string.h>
#include <QDebug>

namespace MREdge {

typedef QSharedPointer<QByteArray> QByteArrayPtr;
typedef QSharedPointer<cv::Mat> cvMatPtr;
typedef QSharedPointer<QImage> QImagePtr;


#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define fDebug qDebug().noquote() << __FILENAME__ << ":"

}

#endif // GLOBAL_H
