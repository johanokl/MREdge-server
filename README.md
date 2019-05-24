Mixed Reality using Edge Computing - Server
========================

### Introduction
The server is to be used with https://github.com/johanokl/MREdge-client

This repository contains a Qt C++ project.

Tested with Ubuntu 18.04.

### Screenshot
![Screenshot 2](../media/screenshots/screenshot1.png?raw=true)

### Setting up the build environment on Ubuntu 18.04
* Install and configure Qt. https://wiki.qt.io/Install_Qt_5_on_Ubuntu  
  ```sudo apt install qt5-default```  
* Install GStreamer:  
  ```sudo apt install libgstreamer1.0-dev```  
  ```sudo apt install libgstreamer-plugins-base1.0-dev```  
  ```sudo apt install gstreamer1.0-plugins-bad```
* Install OpenCV:  
 ```sudo apt install libopencv-dev```
* Install Eigen:  
 ```sudo apt install libeigen3-dev```
* Install GLEW:  
 ```sudo apt install libglew-dev```
* Install OSMesa:  
 ```sudo apt install libosmesa6-dev```
* Install GLU:  
 ```sudo apt install libglu1-mesa-dev```
 
### Building
1. Make sure that you have the build environment set up.
2. Go to the directory with the project file _MREdgeServer.pro_.
3. Run Qt _qmake_ and the system's _make_: ```qmake && make```  
4. If compilation is successful, the executable file is placed in the current directory.

Note: Performance related issues have been observed when running debug builds of the software. If compiling with Qt Creator, test with a release build.


### Running
1. Extract the file _ORBVoc.text_ from _externals/ORB_SLAM2/Vocabulary/ORBvoc.txt.tar.gz_ to the directory where the executable was placed.  
2. Start the server software with:   
```./MREdgeServer```  
To see how you can change the configuration, start the server with:  
```./MREdgeServer --help```  
Two configuration options that are useful for testing are ```./MREdgeServer --display``` to have GUI windows with the content and ```./MREdgeServer --camera <0,1,..>``` to add a local client with video streams from a local webcam. Clicking with the mouse in the windows has the same effect as pressing the _Add_ button in the Android client.

### License
Released under GPL version 3.

### Author
Johan Lindqvist  
johan.lindqvist@gmail.com  
https://github.com/johanokl

