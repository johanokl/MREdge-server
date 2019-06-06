/**
 * Part of the MREdge project.
 * http://github.com/johanokl/MREdge-server
 * Released under GPLv3.
 * Johan Lindqvist (johan.lindqvist@gmail.com)
 */

#include "orbslamprocesser.h"
#include <QDebug>
#include <QImage>
#include <QtMath>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>
#include "System.h"
#include <QJsonDocument>
#include "ViewerAR.h"
#include <QDir>
#include <QThread>
#include <QElapsedTimer>

namespace MREdge {

/**
 * @brief OrbSlamProcesser::OrbSlamProcesser
 * @param session session id
 * @param voc ORBVocabulary file with Bag of Words for image parsing.
 */
OrbSlamProcesser::OrbSlamProcesser(
    qint32 session, ORB_SLAM2::ORBVocabulary *voc,
    bool benchmarking, bool loopClosing)
{
  mSession = session;
  mVocabulary = voc;
  mSLAM = nullptr;
  mViewerAR = nullptr;
  mCalibrateMode = false;
  mLoopClosing = loopClosing;
  mBenchmarking = benchmarking;
  mRunning = true;
}

/**
 * @brief OrbSlamProcesser::~OrbSlamProcesser
 */
OrbSlamProcesser::~OrbSlamProcesser()
{
  mRunning = false;
  if (mSLAM) {
    fDebug << "Shutting down SLAM";
    mSLAM->Shutdown();
  }
  if (mViewerAR) {
    mViewerAR->Stop();
  }
}

/**
 * @brief OrbSlamProcesser::setCalibrateMode
 * @param setEnabled
 */
void OrbSlamProcesser::setCalibrateMode(bool setEnabled)
{
  if (mCalibrateMode != setEnabled) {
    mCalibrateMode = setEnabled;
    if (!mCalibrateMode) {
      calibrateCamera();
    } else {
      mImgSizeCV = cv::Size();
    }
  }
}

/**
 * @brief OrbSlamProcessor::setUserInteractionConfiguration
 * @param config
 */
void OrbSlamProcesser::setUserInteractionConfiguration(QJsonObject config)
{
  if (config.contains("3DObjectType")) {
    m3DObjectType = config.value("3DObjectType").toString();
    if (mViewerAR) {
      mViewerAR->set3DObjectType(m3DObjectType);
    }
  }
}

/**
 * @brief OrbSlamProcesser::setConfig
 * @param calibration
 *
 * Sets up ORB_SLAM2, the rendering threads and calibration data.
 */
void OrbSlamProcesser::setConfig(QJsonObject calibration)
{
  if (mSLAM != nullptr) {
    return;
  }
  // Create SLAM system. It initializes all system threads and gets ready to
  // process frames.
  if (!calibration.contains("Camera.width") ||
      !calibration.contains("Camera.height")) {
    fDebug << "Config data does not contain width or height.";
    return;
  }
  fDebug << "Width: " << calibration.value("Camera.width").toInt();
  fDebug << "Height: " << calibration.value("Camera.height").toInt();

  QSize imageSize = QSize(calibration.value("Camera.width").toInt(),
                          calibration.value("Camera.height").toInt());
  if (imageSize != mImgSize) {
    mViewerAR = new ViewerAR(imageSize.width(), imageSize.height(), mBenchmarking);
    mViewerAR->set3DObjectType(m3DObjectType);
    mSLAM = new ORB_SLAM2::System(
          mVocabulary,
          calibration,
          ORB_SLAM2::System::MONOCULAR,
          false,
          mLoopClosing);

    double fps = calibration.value("Camera.fps").toDouble(30);
    mViewerAR->setSLAM(mSLAM);
    mViewerAR->setFPS(fps);

    float fx = static_cast<float>(calibration.value("Camera.fx").toDouble(517.306408));
    float fy = static_cast<float>(calibration.value("Camera.fy").toDouble(516.469215));
    float cx = static_cast<float>(calibration.value("Camera.cx").toDouble(318.643040));
    float cy = static_cast<float>(calibration.value("Camera.cy").toDouble(255.313989));

    mViewerAR->setCameraCalibration(fx,fy,cx,cy);

    mK = cv::Mat::eye(3,3,CV_32F);
    mK.at<float>(0,0) = fx;
    mK.at<float>(1,1) = fy;
    mK.at<float>(0,2) = cx;
    mK.at<float>(1,2) = cy;

    mDistCoef = cv::Mat::zeros(4, 1, CV_32F);
    mDistCoef.at<float>(0) = static_cast<float>(calibration.value("Camera.k1").toDouble(0.262383));
    mDistCoef.at<float>(1) = static_cast<float>(calibration.value("Camera.k2").toDouble(-0.953104));
    mDistCoef.at<float>(2) = static_cast<float>(calibration.value("Camera.p1").toDouble(-0.005358));
    mDistCoef.at<float>(3) = static_cast<float>(calibration.value("Camera.p2").toDouble(0.002628));
    const float k3 = static_cast<float>(calibration.value("Camera.k3").toDouble(1.163314));
    if (k3 != 0) {
      mDistCoef.resize(5);
      mDistCoef.at<float>(4) = k3;
    }

    mViewerARthread = new std::thread(&ViewerAR::Run, mViewerAR);

    QObject::connect(mViewerAR, &ViewerAR::newImageReady,
                     [=](quint32 frameid, QImagePtr img, int metadata) {
      if (mRunning && mLogTime) {
        processingfinished.insert(frameid, mUptime->nsecsElapsed());
      }
      if (mRunning && mEmitJPEG) {
        emit sendFile(
              mSession, NetworkConnection::File(
                (mEmitMetadata ?
                   NetworkConnection::FileType::IMAGE_WITH_METADATA :
                   NetworkConnection::FileType::IMAGE),
                frameid, jpegFromQImage(*img, mEmitMetadata, metadata)));
      }
      if (mRunning && mEmitQImage) {
        emit sendQImage(mSession, frameid, img);
      }
    });
  }
  mImgSize = imageSize;
}

/**
 * @brief OrbSlamProcesser::setDebugMode
 * @param enable Enabled or disabled.
 *
 * Set the system in debug mode to retrieve more real-time debugging data.
 */
void OrbSlamProcesser::setDebugMode(bool enable)
{
  mDebugMode = enable;
}




/**
 * @brief OrbSlamProcesser::process
 * @param session Session id
 * @param mat OpenCV Mat
 * @param frameid Image id
 *
 * If the instance is in camera calibration mode, the images sent to the internal
 * functions wrapping OpenCV's chessboard camera calibration functions.
 * If it's in normal mode, it's sent to the
 * ORB_SLAM2 SLAM system, to be rendered using the ViewerAR Mixed Reality class.
 */
void OrbSlamProcesser::process(qint32 session, quint32 frameid, cvMatPtr mat)
{
  Q_UNUSED(session);
  if (mCalibrateMode) {
    auto outimg = processImageForCalibration(*mat);
    cv::Mat dstImage;
    cvtColor(outimg, dstImage, cv::COLOR_BGR2RGB);
    if (mEmitJPEG) {
      emit sendFile(mSession, NetworkConnection::File(
                      NetworkConnection::FileType::IMAGE,
                      frameid, jpegFromMat(dstImage)));
    }
    if (mEmitQImage) {
      emit sendQImage(mSession, frameid, QImagePtr(new QImage(qImageFromMat(outimg))));
    }
  } else if (mSLAM) {
    bool colorFrame = false;
    if (mIdentifyColorFrame) {
      colorFrame = true;
      for (int i = 0; i < 10; i++) {
        unsigned char * p = mat->ptr(20 * i,  20 * i);
        if (p[0] != 252 || p[1] != 0 || p[2] != 252) {
           colorFrame = false;
           break;
        }
      }
      if (colorFrame) {
        fDebug << "Color frame found";
      }
    }
    cv::Mat Tcw = mSLAM->TrackMonocular(*mat, frameid);
    int state = mSLAM->GetTrackingState();
    vector<ORB_SLAM2::MapPoint*> vMPs = mSLAM->GetTrackedMapPoints();
    vector<cv::KeyPoint> vKeys = mSLAM->GetTrackedKeyPointsUn();
    mViewerAR->setImagePose(frameid, colorFrame, *mat, Tcw, state, vKeys, vMPs);
    if (mTriggeredA) {
      // Add a 3D object.
      mViewerAR->add3DObject();
      mTriggeredA = false;
    }
    if (mTriggeredB) {
      // Remove the MR 3D object.
      mViewerAR->remove3DObject();
      mTriggeredB = false;
    }
    if (mTriggeredC) {
      // Reset localization.
      mSLAM->Reset();
      mTriggeredC = false;
    }
  }
}

/**
 * @brief OrbSlamProcesser::processImageForCalibration
 * @param image
 * @return OpenCV Mat with chessboard pattern highlighted if found.
 *
 * If action is triggered the image chessboard data is
 * added to the calibration vectors.
 */
cv::Mat OrbSlamProcesser::processImageForCalibration(cv::Mat image) {
  /*
  Problem : Advanced Lane Finding
  A C++ translation of Project 4 - Advanced Lane Finding
  Course: Udacity Self-Driving Car Nanodegree
  Author : Dat Nguyen
  Date   : Feb 24, 2016
  */
  const int CHESS_ROWS = 9;
  const int CHESS_COLS = 6;
  if (mImgSizeCV.area() != 0 && image.size() != mImgSizeCV) {
    fDebug << QString("Size differs. old=(%1x%2), new=(%3x%4)")
              .arg(mImgSizeCV.width).arg(mImgSizeCV.height)
              .arg(image.size().width).arg(image.size().height);
    mArrObjPoints = std::vector<objPoints>();
    mArrImgPoints = std::vector<imgPoints>();
    mOutCamMatrix = cv::Mat(3, 3, CV_32FC1);
    mOutDistCoeffs = cv::Mat();
  }
  if (!image.empty()) {
    // Size of the chess board
    cv::Size board_sz = cv::Size(CHESS_ROWS, CHESS_COLS);
    // Hold corner values returned from findChessCorners
    imgPoints corners;
    // Generate object points
    objPoints corners_3d;
    for (int i = 0; i < CHESS_COLS * CHESS_ROWS; i++) {
      corners_3d.push_back(cv::Point3f(i / CHESS_ROWS, i % CHESS_ROWS, 0.0f));
    }
    // Find Chess corners
    if (cv::findChessboardCorners(
          image, board_sz, corners,
          CV_CALIB_CB_ADAPTIVE_THRESH |
          CV_CALIB_CB_FAST_CHECK |
          CV_CALIB_CB_NORMALIZE_IMAGE)) {
      cv::Mat gray;
      cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
      cv::cornerSubPix(gray, corners,
                       cv::Size(11, 11),  cv::Size(-1, -1),
                       cv::TermCriteria(CV_TERMCRIT_EPS |
                                        CV_TERMCRIT_ITER,
                                        30, 0.1));
      if (mTriggeredA) {
        mTriggeredA = false;
        fDebug << QString("Added image points for image size %1x%2.")
                  .arg(image.size().width).arg(image.size().height);
        mArrImgPoints.push_back(corners);
        mArrObjPoints.push_back(corners_3d);
      }
      cv::drawChessboardCorners(image, board_sz, corners, true);
      cv::circle(image, cv::Point(image.size().width, image.size().height),
                 20, cv::Scalar(0.5, 0.5, 0.5, 0.5));
    }
    mImgSizeCV = image.size();
  }
  return image;
}

/**
 * @brief OrbSlamProcesser::calibrateCamera
 */
void OrbSlamProcesser::calibrateCamera()
{
  if (mArrObjPoints.empty() || mArrImgPoints.empty()) {
    return;
  }
  /*
  Problem : Advanced Lane Finding
  A C++ translation of Project 4 - Advanced Lane Finding
  Course: Udacity Self-Driving Car Nanodegree
  Author : Dat Nguyen
  Date   : Feb 24, 2016
  */
  std::vector<cv::Mat> rotationVectors;
  std::vector<cv::Mat> translationVectors;
  mOutCamMatrix.ptr<float>(0)[0] = 1;
  mOutCamMatrix.ptr<float>(1)[1] = 1;
  cv::calibrateCamera(mArrObjPoints, mArrImgPoints, mImgSizeCV,
                      mOutCamMatrix, mOutDistCoeffs, rotationVectors, translationVectors);
  QJsonObject recordObject;
  recordObject.insert("Camera.width", mImgSizeCV.width);
  recordObject.insert("Camera.height", mImgSizeCV.height);
  recordObject.insert("Camera.fx", mOutCamMatrix.at<double>(0, 0));
  recordObject.insert("Camera.fy", mOutCamMatrix.at<double>(1, 1));
  recordObject.insert("Camera.cx", mOutCamMatrix.at<double>(0, 2));
  recordObject.insert("Camera.cy", mOutCamMatrix.at<double>(1, 2));
  recordObject.insert("Camera.k1", mOutDistCoeffs.at<double>(0));
  recordObject.insert("Camera.k2", mOutDistCoeffs.at<double>(1));
  recordObject.insert("Camera.p1", mOutDistCoeffs.at<double>(2));
  recordObject.insert("Camera.p2", mOutDistCoeffs.at<double>(3));
  recordObject.insert("Camera.k3", (mOutDistCoeffs.size().height > 4) ?
                        mOutDistCoeffs.at<double>(4) : 0);
  setConfig(recordObject);
  QByteArrayPtr ret(new QByteArray(QJsonDocument(recordObject).toJson()));
  fDebug << "Calibration: " << ret->data();
  emit sendFile(mSession, NetworkConnection::File(
                  NetworkConnection::FileType::JSON, 1, ret));
}

}
