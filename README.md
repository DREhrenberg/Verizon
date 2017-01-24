# README #

This README would normally document whatever steps are necessary to get your application up and running.

### What is this repository for? ###

* Quick summary
* Version
* [Learn Markdown](https://bitbucket.org/tutorials/markdowndemo)

### How do I get set up? ###

* Summary of set up

To build the solution, you need to build OpenCV with OpenGL support.  I used Visual Studio 2015 and Windows 7.  Print the following pattern:  http://docs.opencv.org/2.4/_downloads/pattern.png.  It is 9x6 (row/cols), as indicated in the file in_VID5.xml.

The solution, checkerboard-cube, contains two projects.  One is for calibration (using the checkerboard).  The other is for AR.  To use the solution, first run the ‘camera_calibration’ project and use the checkerboard to calibrate your camera.  Then, copy camera_data.xml from the ‘camera_calibration’ project’s directory to the ‘checkerboard-3d-rendering’ project's directory.
I placed the OpenCV project in a directory and set the environment variable OPENCV_DIR as <directory>\install\x64\v14 and added <directory>\install\x64\v14\bin to PATH variable.


* Configuration
* Dependencies
* Database configuration
* How to run tests
* Deployment instructions

### Contribution guidelines ###

* Writing tests
* Code review
* Other guidelines

### Who do I talk to? ###

* Repo owner or admin
* Other community or team contact