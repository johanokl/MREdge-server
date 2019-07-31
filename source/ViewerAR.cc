/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 *
 * To be used with ORB-SLAM2.
 * Includes GPL3 licensed code taken from the ORB-SLAM2 project.
 * Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
 * For more information see <https://github.com/raulmur/ORB_SLAM2>
 */

#include "ViewerAR.h"

#ifndef RENDER_WITH_PANGOLIN
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#define GLAPI __attribute__((visibility("default")))
#include <GL/osmesa.h>
#include <pangolin/display/display_internal.h>
#endif

#include <opencv2/highgui/highgui.hpp>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <QString>
#include <QDateTime>
#include <GL/gl.h>
#include <QElapsedTimer>

using namespace std;

namespace MREdge
{

QStringList ViewerAR::AR_OBJECT_TYPES = QStringList()
    << "CUBE_A" << "CUBE_B" << "CUBE_C";


/**
 * @brief ViewerAR::ViewerAR
 */
ViewerAR::ViewerAR(int width, int height, const bool benchmarking,
                   const bool logTime) {
  mRunning = true;
  mWidth = width;
  mHeight = height;
  m3DObjectType = "CUBE_A";
  mBenchmarking = benchmarking;
  mLogTime = logTime;
  mTotalTrackedPoints = 0;
  mNumFramesDrawn = 0;
}

/**
 * @brief ViewerAR::set3DObjectType
 * @param objectTypeName
 */
void ViewerAR::set3DObjectType(QString objectTypeName)
{
  m3DObjectMutex.lock();
  mPoseReady = true;
  if (!ViewerAR::AR_OBJECT_TYPES.contains(objectTypeName)) {
    objectTypeName = ViewerAR::AR_OBJECT_TYPES.first();
  }
  m3DObjectType = objectTypeName;
  m3DObjectMutex.unlock();
}

/**
 * @brief ViewerAR::Run
 */
void ViewerAR::Run()
{
  cv::Mat im, Tcw;
  int slamStatus;
  vector<cv::KeyPoint> vKeys;
  vector<ORB_SLAM2::MapPoint*> vMPs;
  quint32 frameid;

  while (mRunning) {
    getImagePose(frameid, im, Tcw, slamStatus, vKeys, vMPs);
    if (!im.empty()) {
      mWidth = im.cols;
      mHeight = im.rows;
      break;
    }
  }

  QElapsedTimer processingTimeTimer;
  processingTimeTimer.start();
  qsrand(QTime::currentTime().msecsSinceStartOfDay());

#ifndef DISABLE_IMAGE_OUTPUT
#ifdef RENDER_WITH_PANGOLIN
#ifdef RENDER_PANGOLIN_HEADLESS
  pangolin::CreateWindowAndBind(QString::number(qrand()).toStdString(),
                                mWidth, mHeight, pangolin::Params({{"scheme", "headless"}}));
#else
  pangolin::CreateWindowAndBind(QString::number(qrand()).toStdString(),
                                mWidth, mHeight);
#endif
  pangolin::View& d_image = pangolin::Display("image");
  d_image.SetBounds(0, 1.0f, 0, 1.0f, (float)mWidth / mHeight); // -?
  d_image.SetLock(pangolin::LockLeft, pangolin::LockTop);

  pangolin::GlTexture color_buffer(mWidth, mHeight);
  pangolin::GlRenderBuffer depth_buffer(mWidth, mHeight);
  pangolin::GlFramebuffer fbo_buffer(color_buffer, depth_buffer);
  fbo_buffer.Bind();

#else
  OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGB, 16, 0, 0, nullptr);
  if (!ctx) {
    fDebug << "OSMesaCreateContext failed";
    return;
  }
  // Allocate the image buffer
  GLubyte *buffer = new GLubyte[static_cast<size_t>(mWidth * mHeight) * 3];
  if (!buffer) {
    fDebug << "Alloc image buffer failed";
    return;
  }
  // Bind the buffer to the context and make it current
  if (!OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, mWidth, mHeight )) {
    fDebug << "OSMesaMakeCurrent failed";
  }
#endif

  glEnable(GL_DEPTH_TEST);
  glEnable (GL_BLEND);

  pangolin::GlTexture imageTexture(mWidth, mHeight, GL_RGB, false, 0, GL_RGB, GL_UNSIGNED_BYTE);
  pangolin::OpenGlMatrixSpec P = pangolin::ProjectionMatrixRDF_TopLeft(mWidth, mHeight, fx, fy, cx, cy, 0.001, 1000);

  double cubesize = 0.05;
#endif
  vector<Plane*> m3dObjectPoses;

  mpSystem->DeactivateLocalizationMode();

  qint64 startTime = 0;
  int framesWithObject = 0;

  QMap<int, qint64> framesTime;

  int frames = 0;

  while (mRunning) {
#ifndef DISABLE_IMAGE_OUTPUT
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    frames++;

    // Activate camera view
#ifdef RENDER_WITH_PANGOLIN
    d_image.Activate();
#else
    glViewport(0, 0, mWidth, mHeight);
#endif
#endif

    int arData = 0;

    // Get last image and its computed pose from SLAM
    getImagePose(frameid, im, Tcw, slamStatus, vKeys, vMPs);

    if (mLogTime) {
      startTime = processingTimeTimer.nsecsElapsed();
    }

#ifndef DISABLE_IMAGE_OUTPUT
    glColor3f(1.0, 1.0, 1.0);
    // Draw point cloud
    drawTrackedPoints(vKeys, vMPs, im);
    // Draw image
    drawImageTexture(imageTexture, im);

    glClear(GL_DEPTH_BUFFER_BIT);

    // Load camera projection
    glMatrixMode(GL_PROJECTION);
    P.Load();
    glMatrixMode(GL_MODELVIEW);
    // Load camera pose
    loadCameraPose(Tcw);
#endif

    // Draw virtual things
    if (slamStatus == 2) {
      if (mBenchmarking) {
        if (!m3dObjectPoses.empty()) {
          framesWithObject++;
          if (framesWithObject > 15) {
            mRemove3dObject = true;
          }
        } else {
          framesWithObject = 0;
        }
      }
      if (mAdd3dObject || mRemove3dObject) {
        if (!m3dObjectPoses.empty()) {
          for (size_t i = 0; i < m3dObjectPoses.size(); i++) {
            delete m3dObjectPoses[i];
          }
          m3dObjectPoses.clear();
          fDebug << "All 3D objects erased!";
        }
        mRemove3dObject = false;
      }
      if (mAdd3dObject) {
        Plane* pPlane = detectPlane(Tcw, vMPs, 50);
        if (pPlane) {
          fDebug << "New virtual cube inserted!";
          m3dObjectPoses.push_back(pPlane);
          mAdd3dObject = false;
        }
      }
      if (!m3dObjectPoses.empty()) {        
        // Recompute plane if there has been a loop closure or global BA
        // In localization mode, map is not updated so we do not need to recompute
        bool bRecompute = false;
        if (mpSystem->MapChanged()) {
          fDebug << "Map changed. All virtual elements are recomputed!";
          bRecompute = true;
        }
        for (size_t i = 0; i < m3dObjectPoses.size(); i++) {
          Plane* pPlane = m3dObjectPoses[i];
          if (pPlane) {
            if (bRecompute) {
              pPlane->Recompute();
            }
            arData = 1;
            // Draw cube
#ifndef DISABLE_IMAGE_OUTPUT
            glPushMatrix();
            pPlane->glTpw.Multiply();
            draw3DObject(cubesize);
            glPopMatrix();
#endif
            if (mBenchmarking) {
#ifndef DISABLE_IMAGE_OUTPUT
              glDisable(GL_DEPTH_TEST);
              glMatrixMode(GL_MODELVIEW);
              glLoadIdentity();
              glMatrixMode(GL_PROJECTION);
              glLoadIdentity();
              gluOrtho2D(-1000, 1000, -1000, 1000);
              glLoadIdentity();
              glBegin(GL_POLYGON);
              glColor3f(1, 0, 1);
              glVertex2f(-1.0, 1.0);
              glVertex2f(1.0, 1.0);
              glVertex2f(1.0, 0.5);
              glVertex2f(-1.0, 0.5);
              glEnd();
              glEnable(GL_DEPTH_TEST);
              glMatrixMode(GL_MODELVIEW);
              glLoadIdentity();
#endif
            }
          }
        }
      }
    } else if (mBenchmarking) {
      framesWithObject = 0;
    }
    if (mForceColor) {
      arData = 2;
      glDisable(GL_DEPTH_TEST);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluOrtho2D(-1000, 1000, -1000, 1000);
      glLoadIdentity();
      glBegin(GL_POLYGON);
      glColor3f(0, 1, 0);
      glVertex2f(-1.0, 1.0);
      glVertex2f(1.0, 1.0);
      glVertex2f(1.0, 0.5);
      glVertex2f(-1.0, 0.5);
      glEnd();
      glEnable(GL_DEPTH_TEST);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
    }

#ifdef DISABLE_IMAGE_OUTPUT
    QImage outImage(new QImage());
#else
    QImage outImage(mWidth, mHeight, QImage::Format::Format_RGB888);
    uchar* outimagePtr = outImage.bits();

#ifdef RENDER_WITH_PANGOLIN
#ifdef RENDER_PANGOLIN_HEADLESS
    glFinish();
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, mWidth, mHeight, GL_RGB, GL_UNSIGNED_BYTE, outimagePtr);
    outImage = outImage.mirrored(false, true);
#else
    outimagePtr = nullptr;
#endif
    pangolin::FinishFrame();
#else
    // Retrieved image is bottom to top, left to right and BGR.
    // QImage is top to bottom, left to right and RGB.
    for (int y = mHeight - 1; y >= 0; y--) {
      for (int x = 0; x < mWidth; x++) {
        int i = (y * mWidth + x) * 3 + 2;
        *outimagePtr = buffer[i];
        outimagePtr++;
        i--;
        *outimagePtr = buffer[i];
        outimagePtr++;
        i--;
        *outimagePtr = buffer[i];
        outimagePtr++;
      }
    }
#endif
#endif

    emit newImageReady(frameid, outImage, arData);

    if (mLogTime) {
      framesTime.insert(frames, (processingTimeTimer.nsecsElapsed() - startTime) / 1000000);
    }

#ifndef USE_QMUTEX_AR
    usleep(mT * 1000);
#endif
  }

  QMapIterator<int, qint64> framesTimeIt(framesTime);
  while (framesTimeIt.hasNext()) {
    framesTimeIt.next();
    fDebug << QString("frametime,%1,%2")
              .arg(framesTimeIt.key()).arg(framesTimeIt.value());
  }


#ifndef DISABLE_IMAGE_OUTPUT
#ifndef RENDER_WITH_PANGOLIN
  OSMesaDestroyContext(ctx);
#else
  fbo_buffer.Unbind();
#endif
#endif

  if (mLogTime) {
    fDebug << "------------------------------------------------------";
    fDebug << "Frames drawn: " << mNumFramesDrawn;
    fDebug << "Total tracked points: " << mTotalTrackedPoints;
    fDebug << "Average tracked points per frame: " << mTotalTrackedPoints /
              qMax(mNumFramesDrawn, 1);
    fDebug << "------------------------------------------------------";
  }
}

/**
 * @brief ViewerAR::Stop
 * Stop the thread.
 */
void ViewerAR::Stop()
{
  mRunning = false;
  mMutexPoseImage.lock();
#ifdef USE_QMUTEX_AR
  mQWaitCondition.wakeAll();
#endif
  mMutexPoseImage.unlock();
}

/**
 * @brief ViewerAR::setImagePose
 * @param im
 * @param Tcw
 * @param status
 * @param vKeys
 * @param vMPs
 */
void ViewerAR::setImagePose(
    quint32 id, bool forcecolor,
    const cv::Mat &im, const cv::Mat &Tcw, const int &status,
    const vector<cv::KeyPoint> &vKeys, const vector<ORB_SLAM2::MapPoint*> &vMPs)
{
  mMutexPoseImage.lock();
  mPoseReady = true;
  mFrameId = id;
  mForceColor = forcecolor;
  mImage = im;//.clone();
  mTcw = Tcw;//.clone();
  mStatus = status;
  mvKeys = vKeys;
  mvMPs = vMPs;
#ifdef USE_QMUTEX_AR
  mQWaitCondition.wakeAll();
#endif
  mMutexPoseImage.unlock();
}

/**
 * @brief ViewerAR::getImagePose
 * @param im
 * @param Tcw
 * @param status
 * @param vKeys
 * @param vMPs
 */
void ViewerAR::getImagePose(
    quint32 &frameid, cv::Mat &im, cv::Mat &Tcw, int &status,
    std::vector<cv::KeyPoint> &vKeys, std::vector<ORB_SLAM2::MapPoint*> &vMPs)
{
  mMutexPoseImage.lock();
#ifdef USE_QMUTEX_AR
  if (!mPoseReady) {
    mQWaitCondition.wait(&mMutexPoseImage);
  }
#endif
  mPoseReady = false;
  frameid = mFrameId;
  im = mImage;
  Tcw = mTcw;
  status = mStatus;
  vKeys = mvKeys;
  vMPs = mvMPs;
  mMutexPoseImage.unlock();
}

/**
 * @brief ViewerAR::loadCameraPose
 * @param Tcw
 */
void ViewerAR::loadCameraPose(const cv::Mat &Tcw)
{
  mMutexPoseImage.lock();
  if (Tcw.empty()) {
    mMutexPoseImage.unlock();
    return;
  }
  pangolin::OpenGlMatrix M;
  M.m[0] = Tcw.at<float>(0, 0);
  M.m[1] = Tcw.at<float>(1, 0);
  M.m[2] = Tcw.at<float>(2, 0);
  M.m[3] = 0.0;
  M.m[4] = Tcw.at<float>(0, 1);
  M.m[5] = Tcw.at<float>(1, 1);
  M.m[6] = Tcw.at<float>(2, 1);
  M.m[7] = 0.0;
  M.m[8] = Tcw.at<float>(0, 2);
  M.m[9] = Tcw.at<float>(1, 2);
  M.m[10] = Tcw.at<float>(2, 2);
  M.m[11] = 0.0;
  M.m[12] = Tcw.at<float>(0, 3);
  M.m[13] = Tcw.at<float>(1, 3);
  M.m[14] = Tcw.at<float>(2, 3);
  M.m[15] = 1.0;
  mMutexPoseImage.unlock();
#ifdef RENDER_WITH_PANGOLIN
  M.Load();
#else
  glLoadMatrixd(M.m);
#endif
}

/**
 * @brief ViewerAR::printStatus
 * @param status
 * @param bLocMode
 * @param im
 */
void ViewerAR::drawStatus(const int &status, const bool &bLocMode, cv::Mat &im)
{
  Q_UNUSED(bLocMode);
  switch(status) {
  case 1:
    addTextToImage("", im, 255, 0, 0);
    break;
  case 2:
    addTextToImage("", im, 0, 255, 0);
    break;
  case 3:
    addTextToImage("*", im, 255, 0, 0);
    break;
  }
}

/**
 * @brief ViewerAR::addTextToImage
 * @param s
 * @param im
 * @param r
 * @param g
 * @param b
 */
void ViewerAR::addTextToImage(const string &s, cv::Mat &im, const int r, const int g, const int b)
{
  cv::putText(im, s, cv::Point(10, im.rows - 10), cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(r, g, b), 2, 8);
}

/**
 * @brief ViewerAR::drawImageTexture
 * @param imageTexture
 * @param im
 */
void ViewerAR::drawImageTexture(pangolin::GlTexture &imageTexture, cv::Mat &im)
{
  if (!im.empty())
  {
    imageTexture.Upload(im.data, GL_RGB, GL_UNSIGNED_BYTE);
    imageTexture.RenderToViewportFlipY();
  }
}

/**
 * @brief ViewerAR::draw3DObject
 * @param size
 * @param x
 * @param y
 * @param z
 */
void ViewerAR::draw3DObject(const double &size, const double x, const double y, const double z)
{
  pangolin::OpenGlMatrix M = pangolin::OpenGlMatrix::Translate(-x, -size - y, -z);
  glPushMatrix();
  M.Multiply();
  GLfloat axis_min= static_cast<GLfloat>(-size);
  GLfloat axis_max = static_cast<GLfloat>(size);

  const GLfloat min = axis_min;
  const GLfloat max = axis_max;

  const GLfloat mi2 = min * 2;
  const GLfloat ma2 = max * 2;

  const GLfloat mi3 = min * 3;
  const GLfloat ma3 = max * 3;

  const GLfloat verts[] = {
    mi2, min, max,  ma2, min, max,  mi2, max, max,  ma2, max, max,  // FRONT
    mi2, min, min,  mi2, max, min,  ma2, min, min,  ma2, max, min,  // BACK

    mi2, min, max, /**/ mi2, mi2, 0, /**/ mi2, max, max,  mi2, min, min,  mi2, max, min,  // LEFT
    ma2, min, min, /**/ ma2, mi2, 0, /**/ ma2, max, min,  ma2, min, max,  ma2, max, max,  // RIGHT

    mi2, max, max,  ma2, max, max,  mi2, max, min,  ma2, max, min,  // TOP

    mi3, max, ma2,  ma3, max, ma2,  mi3, max, mi2,  ma3, max, mi2,  // GRASS

    mi2, min, max,  ma2, min, max,  mi2, mi2, 0,    ma2, mi2, 0,    // ROOF 1
    mi2, mi2, 0,    ma2, mi2, 0,    mi2, min, min,  ma2, min, min,  // ROOF 2
  };

  glVertexPointer(3, GL_FLOAT, 0, verts);
  glEnableClientState(GL_VERTEX_ARRAY);

  QColor colora;
  QColor colorb;
  QColor colorc;

  m3DObjectMutex.lock();
  switch (ViewerAR::AR_OBJECT_TYPES.indexOf(m3DObjectType)) {
  case 0:
    colora = QColor(206, 239, 255);
    colorb = QColor(223, 245, 255);
    colorc = QColor(239, 65, 53);
    break;
  case 1:
    colora = QColor(249, 205, 48);
    colorb = QColor(255, 255, 255);
    colorc = QColor(22, 101, 161);
    break;
  default:
    colora = QColor(255, 0, 0);
    colorb = QColor(0, 0, 255);
    colorc = QColor(0, 0, 255);
  }
  m3DObjectMutex.unlock();

  glColor4f(colora.redF(), colora.greenF(), colora.blueF(), 1.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);

  glColor4f(colorb.redF(), colorb.greenF(), colorb.blueF(), 1.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 8, 5);
  glDrawArrays(GL_TRIANGLE_STRIP, 13, 5);

  //glColor4f(colorc.redF(), colorc.greenF(), colorc.blueF(), 1.0f);
  //glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);

  glColor4f(0, 0.8f, 0, 1.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 22, 4);

  glColor4f(0.8f, 0.5f, 0, 1.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 26, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 30, 4);

  glDisableClientState(GL_VERTEX_ARRAY);
  glPopMatrix();
}

/**
 * @brief ViewerAR::drawTrackedPoints
 * @param vKeys Tracked points
 * @param vMPs Significant Point cloud points
 * @param im
 */
void ViewerAR::drawTrackedPoints(const std::vector<cv::KeyPoint> &vKeys, const std::vector<ORB_SLAM2::MapPoint *> &vMPs, cv::Mat &im)
{
  int mTotalTrackedPointsThisFrame = 0;
  const size_t N = vKeys.size();
  for (size_t i = 0; i < N; i++) {
    if (vMPs[i]) {
      mTotalTrackedPointsThisFrame++;
      cv::circle(im, vKeys[i].pt, 1, cv::Scalar(0, 255, 0), -1);
    }
  }
  mTotalTrackedPoints += mTotalTrackedPointsThisFrame;
  mNumFramesDrawn += 1;
}

/**
 * @brief ViewerAR::setCameraCalibration
 * @param fx_
 * @param fy_
 * @param cx_
 * @param cy_
 */
void ViewerAR::setCameraCalibration(const float &fx_, const float &fy_, const float &cx_, const float &cy_) {
  fx = static_cast<double>(fx_);
  fy = static_cast<double>(fy_);
  cx = static_cast<double>(cx_);
  cy = static_cast<double>(cy_);
}


/**
 * @brief ViewerAR::detectPlane
 * @param Tcw
 * @param vMPs Point Cloud points
 * @param iterations
 * @return
 */
ViewerAR::Plane* ViewerAR::detectPlane(const cv::Mat Tcw, const std::vector<ORB_SLAM2::MapPoint*> &vMPs, const int iterations)
{
  // Retrieve 3D points
  vector<cv::Mat> vPoints;
  vPoints.reserve(vMPs.size());
  vector<ORB_SLAM2::MapPoint *> vPointMP;
  vPointMP.reserve(vMPs.size());
  for(size_t i=0; i < vMPs.size(); i++) {
    ORB_SLAM2::MapPoint * pMP = vMPs[i];
    if (pMP) {
      if (pMP->Observations() > 5) {
        vPoints.push_back(pMP->GetWorldPos());
        vPointMP.push_back(pMP);
      }
    }
  }
  const int N = vPoints.size();
  if (N < 50) {
    return nullptr;
  }
  // Indices for minimum set selection
  vector<size_t> vAllIndices;
  vAllIndices.reserve(N);
  vector<size_t> vAvailableIndices;
  for (int i = 0; i < N; i++) {
    vAllIndices.push_back(i);
  }
  float bestDist = 1e10;
  vector<float> bestvDist;
  //RANSAC
  for (int n=0; n<iterations; n++) {
    vAvailableIndices = vAllIndices;
    cv::Mat A(3, 4, CV_32F);
    A.col(3) = cv::Mat::ones(3, 1, CV_32F);
    // Get min set of points
    for (short i = 0; i < 3; ++i) {
      int randi = DUtils::Random::RandomInt(0, vAvailableIndices.size() - 1);
      int idx = vAvailableIndices[randi];
      A.row(i).colRange(0, 3) = vPoints[idx].t();
      vAvailableIndices[randi] = vAvailableIndices.back();
      vAvailableIndices.pop_back();
    }
    cv::Mat u, w, vt;
    cv::SVDecomp(A, w, u, vt, cv::SVD::MODIFY_A | cv::SVD::FULL_UV);
    const float a = vt.at<float>(3, 0);
    const float b = vt.at<float>(3, 1);
    const float c = vt.at<float>(3, 2);
    const float d = vt.at<float>(3, 3);
    vector<float> vDistances(N, 0);
    const float f = 1.0f / sqrt(a * a + b * b + c * c + d * d);
    for(int i = 0; i < N; i++) {
      vDistances[i] = fabs(
            vPoints[i].at<float>(0) * a +
            vPoints[i].at<float>(1) * b +
            vPoints[i].at<float>(2) * c +
            d) * f;
    }
    vector<float> vSorted = vDistances;
    sort(vSorted.begin(),vSorted.end());
    int nth = max((int)(0.2 * N), 20);
    const float medianDist = vSorted[nth];
    if (medianDist<bestDist) {
      bestDist = medianDist;
      bestvDist = vDistances;
    }
  }
  // Compute threshold inlier/outlier
  const float th = 1.4 * bestDist;
  vector<bool> vbInliers(N, false);
  int nInliers = 0;
  for (int i = 0; i < N; i++) {
    if (bestvDist[i] < th) {
      nInliers++;
      vbInliers[i] = true;
    }
  }
  vector<ORB_SLAM2::MapPoint *> vInlierMPs(nInliers, nullptr);
  int nin = 0;
  for (int i = 0; i < N; i++) {
    if (vbInliers[i]) {
      vInlierMPs[nin] = vPointMP[i];
      nin++;
    }
  }
  return new Plane(vInlierMPs, Tcw);
}

/**
 * @brief ViewerAR::Plane::Plane
 * @param vMPs Point cloud points
 * @param Tcw
 */
ViewerAR::Plane::Plane(const std::vector<ORB_SLAM2::MapPoint *> &vMPs, const cv::Mat &Tcw)
  : mvMPs(vMPs), mTcw(Tcw.clone())
{
  rang = -3.14f / 2 + ((float)rand() / RAND_MAX) * 3.14f;
  Recompute();
}

/**
 * @brief ViewerAR::Plane::Recompute
 */
void ViewerAR::Plane::Recompute()
{
  const int N = mvMPs.size();
  // Recompute plane with all points
  cv::Mat A = cv::Mat(N, 4, CV_32F);
  A.col(3) = cv::Mat::ones(N, 1, CV_32F);
  o = cv::Mat::zeros(3, 1, CV_32F);
  int nPoints = 0;
  for (int i=0; i<N; i++) {
    ORB_SLAM2::MapPoint* pMP = mvMPs[i];
    if (!pMP->isBad()) {
      cv::Mat Xw = pMP->GetWorldPos();
      o+=Xw;
      A.row(nPoints).colRange(0, 3) = Xw.t();
      nPoints++;
    }
  }
  A.resize(nPoints);
  cv::Mat u, w, vt;
  cv::SVDecomp(A, w, u, vt, cv::SVD::MODIFY_A | cv::SVD::FULL_UV);
  float a = vt.at<float>(3, 0);
  float b = vt.at<float>(3, 1);
  float c = vt.at<float>(3, 2);
  o = o * (1.0f / nPoints);
  const float f = 1.0f / sqrt(a * a + b * b + c * c);
  // Compute XC just the first time
  if (XC.empty()) {
    cv::Mat Oc = -mTcw.colRange(0, 3).rowRange(0, 3).t() * mTcw.rowRange(0, 3).col(3);
    XC = Oc-o;
  }
  if ((XC.at<float>(0) * a + XC.at<float>(1) * b + XC.at<float>(2) * c) > 0) {
    a = -a;
    b = -b;
    c = -c;
  }
  const float nx = a * f;
  const float ny = b * f;
  const float nz = c * f;
  n = (cv::Mat_<float>(3, 1)<<nx, ny, nz);
  cv::Mat up = (cv::Mat_<float>(3, 1) << 0.0f, 1.0f, 0.0f);
  cv::Mat v = up.cross(n);
  const float sa = cv::norm(v);
  const float ca = up.dot(n);
  const float ang = atan2(sa, ca);
  Tpw = cv::Mat::eye(4, 4, CV_32F);
  Tpw.rowRange(0, 3).colRange(0, 3) = ExpSO3(v * ang / sa) * ExpSO3(up * rang);
  o.copyTo(Tpw.col(3).rowRange(0, 3));
  glTpw.m[0] = Tpw.at<float>(0, 0);
  glTpw.m[1] = Tpw.at<float>(1, 0);
  glTpw.m[2] = Tpw.at<float>(2, 0);
  glTpw.m[3] = 0.0;
  glTpw.m[4] = Tpw.at<float>(0, 1);
  glTpw.m[5] = Tpw.at<float>(1, 1);
  glTpw.m[6] = Tpw.at<float>(2, 1);
  glTpw.m[7] = 0.0;
  glTpw.m[8] = Tpw.at<float>(0, 2);
  glTpw.m[9] = Tpw.at<float>(1, 2);
  glTpw.m[10] = Tpw.at<float>(2, 2);
  glTpw.m[11] = 0.0;
  glTpw.m[12] = Tpw.at<float>(0, 3);
  glTpw.m[13] = Tpw.at<float>(1, 3);
  glTpw.m[14] = Tpw.at<float>(2, 3);
  glTpw.m[15] = 1.0;
}


/**
 * @brief ViewerAR::Plane::Plane
 * @param nx
 * @param ny
 * @param nz
 * @param ox
 * @param oy
 * @param oz
 */
ViewerAR::Plane::Plane(const float &nx, const float &ny, const float &nz, const float &ox, const float &oy, const float &oz)
{
  n = (cv::Mat_<float>(3, 1) << nx, ny, nz);
  o = (cv::Mat_<float>(3, 1) << ox, oy, oz);
  cv::Mat up = (cv::Mat_<float>(3, 1) << 0.0f, 1.0f, 0.0f);
  cv::Mat v = up.cross(n);
  const float s = cv::norm(v);
  const float c = up.dot(n);
  const float a = atan2(s, c);
  Tpw = cv::Mat::eye(4, 4, CV_32F);
  const float rang = -3.14f / 2 + ((float)rand() / RAND_MAX) * 3.14f;
  cout << rang;
  Tpw.rowRange(0,3).colRange(0, 3) = ExpSO3(v * a / s)*ExpSO3(up * rang);
  o.copyTo(Tpw.col(3).rowRange(0, 3));
  glTpw.m[0] = Tpw.at<float>(0, 0);
  glTpw.m[1] = Tpw.at<float>(1, 0);
  glTpw.m[2] = Tpw.at<float>(2, 0);
  glTpw.m[3] = 0.0;
  glTpw.m[4] = Tpw.at<float>(0, 1);
  glTpw.m[5] = Tpw.at<float>(1, 1);
  glTpw.m[6] = Tpw.at<float>(2, 1);
  glTpw.m[7] = 0.0;
  glTpw.m[8] = Tpw.at<float>(0, 2);
  glTpw.m[9] = Tpw.at<float>(1, 2);
  glTpw.m[10] = Tpw.at<float>(2, 2);
  glTpw.m[11] = 0.0;
  glTpw.m[12] = Tpw.at<float>(0, 3);
  glTpw.m[13] = Tpw.at<float>(1, 3);
  glTpw.m[14] = Tpw.at<float>(2, 3);
  glTpw.m[15] = 1.0;
}

/**
 * @brief ExpSO3
 * @param x
 * @param y
 * @param z
 * @return
 */
cv::Mat ViewerAR::Plane::ExpSO3(const float &x, const float &y, const float &z)
{
  cv::Mat I = cv::Mat::eye(3, 3, CV_32F);
  const float d2 = x * x + y * y + z * z;
  const float d = sqrt(d2);
  cv::Mat W = (cv::Mat_<float>(3, 3) <<
               0, -z, y,
               z, 0, -x,
               -y, x, 0);
  const float eps = 1e-4;
  if (d < eps) {
    return (I + W + 0.5f * W * W);
  }
  return (I + W * sin(d) / d + W * W * (1.0f - cos(d)) / d2);
}

/**
 * @brief ExpSO3
 * @param v
 * @return
 */
cv::Mat ViewerAR::Plane::ExpSO3(const cv::Mat &v)
{
  return ExpSO3(v.at<float>(0), v.at<float>(1), v.at<float>(2));
}

}

#ifndef RENDER_WITH_PANGOLIN
namespace pangolin
{
const GLubyte gNotErrorLookup[] = "XX";
const GLubyte* glErrorString(GLenum) {return gNotErrorLookup; }
bool ShouldQuit() { return true; }
void Handler::Keyboard(View&, unsigned char, int, int, bool) {}
void HandlerScroll::Mouse(View&, MouseButton, int, int, bool, int) {}
void Handler::Mouse(View&, MouseButton, int, int, bool, int) {}
void Handler::MouseMotion(View&, int, int, int) {}
void Handler::PassiveMouseMotion(View&, int, int, int) {}
void HandlerScroll::Special(View&, InputSpecial, float, float, float, float, float, float, int) {}
void Handler::Special(View&, InputSpecial, float, float, float, float, float, float, int) {}
extern PangolinGl dummyContext;
}
#endif
