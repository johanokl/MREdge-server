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

#ifndef VIEWERAR_H
#define VIEWERAR_H

#include <mutex>
#include <opencv2/core/core.hpp>
#include <pangolin/pangolin.h>
#include <string>
#include "System.h"
#include <QImage>
#include "global.h"
#include <QMutex>
#include <QWaitCondition>

namespace MREdge
{

/**
 * @brief The ViewerAR class
 */
class ViewerAR : public QObject {

  Q_OBJECT

public:
  class Plane {
  public:
    Plane(const std::vector<ORB_SLAM2::MapPoint*> &vMPs, const cv::Mat &Tcw);
    Plane(const float &nx, const float &ny, const float &nz, const float &ox, const float &oy, const float &oz);
    void Recompute();
    //transformation from world to the plane
    pangolin::OpenGlMatrix glTpw;
  private:
    cv::Mat ExpSO3(const float &x, const float &y, const float &z);
    cv::Mat ExpSO3(const cv::Mat &v);
    //normal
    cv::Mat n;
    //origin
    cv::Mat o;
    //arbitrary orientation along normal
    float rang;
    //transformation from world to the plane
    cv::Mat Tpw;
    //MapPoints that define the plane
    std::vector<ORB_SLAM2::MapPoint*> mvMPs;
    //camera pose when the plane was first observed (to compute normal direction)
    cv::Mat mTcw, XC;
  };

  static QStringList AR_OBJECT_TYPES;

  ViewerAR(int width, int height, const bool benchmarking);

  void setFPS(const double fps){
    mFPS = fps;
    mT = 1e3/fps;
  }

  void setSLAM(ORB_SLAM2::System* pSystem){
    mpSystem = pSystem;
  }

  // Main thread function.
  void Run();
  void Stop();

  void setCameraCalibration(const float &fx_, const float &fy_, const float &cx_, const float &cy_) {
    fx = static_cast<double>(fx_);
    fy = static_cast<double>(fy_);
    cx = static_cast<double>(cx_);
    cy = static_cast<double>(cy_);
  }

  void setImagePose(const cv::Mat &im, const cv::Mat &Tcw, const int &status,
                    const std::vector<cv::KeyPoint> &vKeys, const std::vector<ORB_SLAM2::MapPoint*> &vMPs);

  void getImagePose(cv::Mat &im, cv::Mat &Tcw, int &status,
                    std::vector<cv::KeyPoint> &vKeys,  std::vector<ORB_SLAM2::MapPoint*> &vMPs);

  void set3DObjectType(QString object);
  void add3DObject() {
    mRemove3dObject = true;
    mAdd3dObject = true;
  }

  void remove3DObject() {
    mRemove3dObject = true;
  }


signals:
  // Extend second parameter from int to larger data structure
  // if needed to store larger.
  void newImageReady(QImagePtr image, int metadata);

private:
  void drawStatus(const int &status, const bool &bLocMode, cv::Mat &im);
  void addTextToImage(const std::string &s, cv::Mat &im, const int r=0, const int g=0, const int b=0);
  void loadCameraPose(const cv::Mat &Tcw);
  void drawImageTexture(pangolin::GlTexture &imageTexture, cv::Mat &im);
  void draw3DObject(const double &size, const double x=0, const double y=0, const double z=0);
  void drawTrackedPoints(const std::vector<cv::KeyPoint> &vKeys, const std::vector<ORB_SLAM2::MapPoint*> &vMPs, cv::Mat &im);
  Plane* detectPlane(const cv::Mat Tcw, const std::vector<ORB_SLAM2::MapPoint*> &vMPs, const int iterations=50);

  bool mRunning;

  //SLAM
  ORB_SLAM2::System* mpSystem;

  // frame rate
  double mFPS, mT;
  // calibration
  double fx, fy, cx, cy;

  // Last processed image and computed pose by the SLAM
#ifdef USE_QMUTEX_AR
  QMutex mMutexPoseImage;
  QMutex m3DObjectMutex;
  QWaitCondition mQWaitCondition;
#else
  std::mutex mMutexPoseImage;
  std::mutex m3DObjectMutex;
#endif
  bool mPoseReady = true;
  cv::Mat mTcw;
  cv::Mat mImage;
  int mStatus;
  std::vector<cv::KeyPoint> mvKeys;
  std::vector<ORB_SLAM2::MapPoint*> mvMPs;
  bool mAdd3dObject = false;
  bool mRemove3dObject = false;
  int mWidth;
  int mHeight;

  QString m3DObjectType;
  bool mBenchmarking;
};


}


#endif // VIEWERAR_H


