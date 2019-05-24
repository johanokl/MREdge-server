/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "networkconnection.h"
#include <mutex>

namespace MREdge {

/**
 * @brief NetworkConnection::setSendImagesForSession
 * @param session
 * @param enabled
 */
void NetworkConnection::setSendImagesForSession(qint32 session, bool enabled)
{
  mSendImagesForSessionListMutex.lock();
  mSendImagesForSessionList.insert(session, enabled);
  mSendImagesForSessionListMutex.unlock();
}

/**
 * @brief NetworkConnection::sendImagesForSession
 * @param sessionId
 * @return
 */
bool NetworkConnection::sendImagesForSession(qint32 sessionId)
{
  mSendImagesForSessionListMutex.lock();
  bool retval = mSendImagesForSessionList.value(sessionId, false);
  mSendImagesForSessionListMutex.unlock();
  return retval;
}

}