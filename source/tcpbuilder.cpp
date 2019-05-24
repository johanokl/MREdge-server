/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "tcpbuilder.h"
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QtMath>

namespace MREdge {

/**
 * @brief TcpBuilder::TcpBuilder
 * @param session
 */
TcpBuilder::TcpBuilder(qint32 session)
{
  mSession = session;
  mNumReceivedBytes = 0;
  mData = QByteArrayPtr(new QByteArray);
  mFileTransferSize = -1;
  mBufferSent = true;
  mBufferInitialized = false;
}

/**
 * @brief TcpBuilder::readData
 * @param session Session id.
 * @param data New data in this packet
 *
 * Appends the new TCP packet and parses the data.
 */
void TcpBuilder::readData(qint32 session, QByteArrayPtr data)
{
  if (session != mSession) {
    // The received signal was not for this session. Ignore it.
    return;
  }
  if (!data.isNull()) {
    // Append the received byte to concatenated byte array.
    mData->append(*data);
    mNumReceivedBytes = mData->size();
  }
  if (mBufferInitialized &&
      mNumReceivedBytes >= mFileTransferSize &&
      !mBufferSent) {
    // We have received all the bytes for this object as
    // defined in its header. Create a new byte array with all
    // the content and send it out.
    QByteArrayPtr retArray(new QByteArray(*mData));
    if (retArray->size() > mFileTransferSize) {
      retArray->truncate(mFileTransferSize);
    }
    mData->remove(0, mFileTransferSize);
    emit fileReady(mSession, NetworkConnection::File(
                     mFileTransferType, mFileTransferId, retArray));
    mNumReceivedBytes = 0;
    mFileTransferSize = -1;
    mBufferInitialized = false;
    mBufferSent = true;
  }
  if (!mBufferInitialized && mData->size() >= 10) {
    // We have received a new header. Parse it and store its content.
    QDataStream dstream(mData.data(), QIODevice::OpenModeFlag::ReadOnly);
    dstream.setByteOrder(QDataStream::BigEndian);
    dstream >> mFileTransferSize;
    dstream >> mFileTransferType;
    dstream >> mFileTransferId;
    mData->remove(0, 10);
    mNumReceivedBytes = mData->size();
    mBufferInitialized = true;
    mBufferSent = false;
    if (mNumReceivedBytes >= mFileTransferSize) {
      // All the content of the new file is already in the stored byte array.
      // Rerun the function so that the file can be sent immediately.
      readData(mSession, nullptr);
    }
  }
}

}
