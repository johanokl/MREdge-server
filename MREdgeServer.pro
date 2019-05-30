#===========================================================
#
#                         MREdge
#  Server for creating mixed reality using edge computing.
#       Johan Lindqvist (johan.lindqvist@gmail.com)
#
#         ---------------------------------------
#
# > For testing: Enable this to include support for
#   creating GUI windows that display the video source
#   feed and generated output, both for connected
#   devices and the local mock camera client.
#   Note that this also adds dependencies on Qt5 widgets
#   and the surrounding environment.
# > Uncomment following line:
CONFIG += ENABLE_GUI_SUPPORT
#
# > Use Pangolin 3D library
# > Uncomment following line:
# CONFIG += USE_PANGOLIN
# CONFIG += SET_PANGOLIN_HEADLESS_MODE
#
# > No graphics is sent, only limited information
#   about  pointcloud and AR object.
# > Uncomment following line:
#CONFIG += DISABLE_IMAGE_OUTPUT
#
# > Limit rendering to 1 frame out per 1 frame in.
# > If not set it will render as many frames as possible.
# > Uncomment following line:
CONFIG += LIMIT_RENDERING
#
#
#===========================================================

TEMPLATE = app
CONFIG += qt

QT += network \
    gui \
    core

PKGCONFIG += opencv
PKGCONFIG += gstreamer-1.0
PKGCONFIG += gstreamer-plugins-base-1.0
PKGCONFIG += gstreamer-app-1.0
PKGCONFIG += eigen3
PKGCONFIG += glew

INCLUDEPATH += externals/ORB_SLAM2
INCLUDEPATH += externals/ORB_SLAM2/include
INCLUDEPATH += externals/Pangolin/include
INCLUDEPATH += externals/Pangolin-config

#----------------------------------------

LIMIT_RENDERING {
    # Enable use of QMutex in rendering thread.
    DEFINES += "USE_QMUTEX_AR=TRUE"
    # Enable use of QMutex in local mapping thread.
    #DEFINES += "USE_QMUTEX_LOC_MAP=TRUE"
}

DISABLE_IMAGE_OUTPUT {
    DEFINES += "DISABLE_IMAGE_OUTPUT=TRUE"
}

USE_PANGOLIN {
    DEFINES += "RENDER_WITH_PANGOLIN=TRUE"
    LIBS += -lpangolin
} else {
    SOURCES += externals/Pangolin/src/display/opengl_render_state.cpp
    PKGCONFIG += osmesa
    LIBS += -lGLU
    LIBS += -lOSMesa
    LIBS += -lm
}

SET_PANGOLIN_HEADLESS_MODE {
    DEFINES += "RENDER_PANGOLIN_HEADLESS=TRUE"
}

ENABLE_GUI_SUPPORT {
    DEFINES += "ENABLE_WIDGET_SUPPORT=TRUE"
    QT += widgets
}

#----------------------------------------

DEFINES += "EIGEN_NO_DEPRECATED_WARNING"

SOURCES += \
    source/cannyfilter.cpp \
    source/imageprocesser.cpp \
    source/ViewerAR.cc \
    source/main.cpp \
    source/mrserver.cpp \
    source/udpconnection.cpp \
    source/udpbuilder.cpp \
    source/tcpconnection.cpp \
    source/tcpbuilder.cpp \
    source/mockclient.cpp \
    source/imagewriter.cpp \
    source/udpsender.cpp \
    source/networkconnection.cpp \
    source/videoreceiver.cpp \
    source/videotransmitter.cpp \
    source/orbslamprocesser.cpp \
    source/echoimage.cpp


HEADERS += \
    source/cannyfilter.h \
    source/imageprocesser.h \
    source/ViewerAR.h \
    source/mrserver.h \
    source/global.h \
    source/udpconnection.h \
    source/networkconnection.h \
    source/udpbuilder.h \
    source/tcpconnection.h \
    source/tcpbuilder.h \
    source/mockclient.h \
    source/imagewriter.h \
    source/udpsender.h \
    source/videoreceiver.h \
    source/videotransmitter.h \
    source/orbslamprocesser.h \
    source/echoimage.h


SOURCES += \
    externals/ORB_SLAM2/src/KeyFrameDatabase.cc \
    externals/ORB_SLAM2/src/Tracking.cc \
    externals/ORB_SLAM2/src/ORBmatcher.cc \
    externals/ORB_SLAM2/src/Sim3Solver.cc \
    externals/ORB_SLAM2/src/ORBextractor.cc \
    externals/ORB_SLAM2/src/Map.cc \
    externals/ORB_SLAM2/src/Initializer.cc \
    externals/ORB_SLAM2/src/MapPoint.cc \
    externals/ORB_SLAM2/src/LocalMapping.cc \
    externals/ORB_SLAM2/src/Optimizer.cc \
    externals/ORB_SLAM2/src/LoopClosing.cc \
    externals/ORB_SLAM2/src/Frame.cc \
    externals/ORB_SLAM2/src/KeyFrame.cc \
    externals/ORB_SLAM2/src/MapDrawer.cc \
    externals/ORB_SLAM2/src/Converter.cc \
    externals/ORB_SLAM2/src/System.cc \
    externals/ORB_SLAM2/src/PnPsolver.cc \
    externals/ORB_SLAM2/src/Viewer.cc \
    externals/ORB_SLAM2/src/FrameDrawer.cc \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/se3_ops.hpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_binary_edge.hpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/sparse_block_matrix.hpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_unary_edge.hpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_vertex.hpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_multi_edge.hpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/block_solver.hpp \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/BowVector.cpp \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/FeatureVector.cpp \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/ScoringObject.cpp \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/FORB.cpp \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DUtils/Random.cpp \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DUtils/Timestamp.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/types_six_dof_expmap.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/types_sba.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/types_seven_dof_expmap.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/sparse_optimizer.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimizable_graph.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_gauss_newton.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/parameter.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/batch_stats.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/matrix_structure.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/robust_kernel_factory.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_levenberg.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/robust_kernel_impl.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/hyper_dijkstra.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_dogleg.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/cache.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/hyper_graph_action.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/marginal_covariance_cholesky.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/jacobian_workspace.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/robust_kernel.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/estimate_propagator.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_factory.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/factory.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/parameter_container.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/hyper_graph.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_with_hessian.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/solver.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/string_tools.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/property.cpp \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/timeutil.cpp

HEADERS += \
    externals/ORB_SLAM2/include/FrameDrawer.h \
    externals/ORB_SLAM2/include/MapPoint.h \
    externals/ORB_SLAM2/include/KeyFrame.h \
    externals/ORB_SLAM2/include/Viewer.h \
    externals/ORB_SLAM2/include/KeyFrameDatabase.h \
    externals/ORB_SLAM2/include/Map.h \
    externals/ORB_SLAM2/include/ORBextractor.h \
    externals/ORB_SLAM2/include/ORBmatcher.h \
    externals/ORB_SLAM2/include/Frame.h \
    externals/ORB_SLAM2/include/LocalMapping.h \
    externals/ORB_SLAM2/include/Converter.h \
    externals/ORB_SLAM2/include/PnPsolver.h \
    externals/ORB_SLAM2/include/Optimizer.h \
    externals/ORB_SLAM2/include/LoopClosing.h \
    externals/ORB_SLAM2/include/Tracking.h \
    externals/ORB_SLAM2/include/Initializer.h \
    externals/ORB_SLAM2/include/ORBVocabulary.h \
    externals/ORB_SLAM2/include/System.h \
    externals/ORB_SLAM2/include/Sim3Solver.h \
    externals/ORB_SLAM2/include/MapDrawer.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/FeatureVector.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/BowVector.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/TemplatedVocabulary.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/ScoringObject.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/FClass.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DBoW2/FORB.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DUtils/Timestamp.h \
    externals/ORB_SLAM2/Thirdparty/DBoW2/DUtils/Random.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/types_seven_dof_expmap.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/se3quat.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/se3_ops.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/types_sba.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/types_six_dof_expmap.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/types/sim3.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_factory.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/jacobian_workspace.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_dogleg.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/robust_kernel_impl.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/estimate_propagator.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_gauss_newton.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/sparse_block_matrix_ccs.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_binary_edge.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/hyper_graph.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/hyper_graph_action.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/matrix_structure.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/robust_kernel.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/hyper_dijkstra.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/parameter.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/matrix_operations.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/sparse_block_matrix_diagonal.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/solver.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/cache.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_unary_edge.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/batch_stats.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimizable_graph.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/creators.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/openmp_mutex.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_edge.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/eigen_types.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_with_hessian.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/block_solver.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_property.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/linear_solver.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/sparse_optimizer.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/robust_kernel_factory.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/parameter_container.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/sparse_block_matrix.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_multi_edge.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/marginal_covariance_cholesky.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/base_vertex.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/optimization_algorithm_levenberg.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/core/factory.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/solvers/linear_solver_dense.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/solvers/linear_solver_eigen.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/misc.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/os_specific.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/string_tools.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/timeutil.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/macros.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/property.h \
    externals/ORB_SLAM2/Thirdparty/g2o/g2o/stuff/color_macros.h


CONFIG -= no-pkg-config
CONFIG += link_pkgconfig


