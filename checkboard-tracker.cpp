// David Ehrenberg
// January 12, 2016
// Verizon Augmented Reality Demo


#include <iostream>
#include <queue>
#include <string>

#include <stdio.h>

#include "opencv2/core.hpp"
#include "opencv2/core/opengl.hpp"
#include "opencv2/core/ocl.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/highgui.hpp"

#include <Windows.h>
#include <gl/GL.h>

using namespace cv;
using namespace std;

const float zNear = 0.05;
const float zFar = 500.0;

// Context data to send to on_opengl(), this function is called each time the window is updated
struct contextdata {
	Mat *frame;
	Mat *outImg;
	bool drawObject;
	Mat *projection;
	Mat *modelview;
	int64 startTick;
};

// Convert OpenCV matrix into opengl’s matrix
GLfloat* convertMatrixType(const cv::Mat& m)
{
	typedef double precision;

	Size s = m.size();
	GLfloat* mGL = new GLfloat[s.width*s.height];

	for (int ix = 0; ix < s.width; ix++)
	{
		for (int iy = 0; iy < s.height; iy++)
		{
			mGL[ix*s.height + iy] = m.at<precision>(iy, ix);
		}
	}

	return mGL;
}

// Generate projection and modelview matrix from intrinsic and extrinsic matrices
void generateProjectionModelview(const Mat& calibration, const Mat& rotation, const Mat& translation, Mat& projection, Mat& modelview)
{
	projection.setTo(0.0);
	typedef double precision;
	double fx = calibration.at<precision>(0, 0);
	double fy = calibration.at<precision>(1, 1);
	double cx = calibration.at<precision>(0, 2);
	double cy = calibration.at<precision>(1, 2);

	projection.at<precision>(0, 0) = fx / cx;
	projection.at<precision>(1, 1) = fy / cy;
	projection.at<precision>(2, 2) = (zNear + zFar) / (zNear - zFar);
	projection.at<precision>(2, 3) = 2 * zNear*zFar / (zNear - zFar);
	projection.at<precision>(3, 2) = -1.0;

	modelview.at<precision>(0, 0) = rotation.at<precision>(0, 0);
	modelview.at<precision>(1, 0) = rotation.at<precision>(1, 0);
	modelview.at<precision>(2, 0) = rotation.at<precision>(2, 0);
	modelview.at<precision>(3, 0) = 0;

	modelview.at<precision>(0, 1) = rotation.at<precision>(0, 1);
	modelview.at<precision>(1, 1) = rotation.at<precision>(1, 1);
	modelview.at<precision>(2, 1) = rotation.at<precision>(2, 1);
	modelview.at<precision>(3, 1) = 0;

	modelview.at<precision>(0, 2) = rotation.at<precision>(0, 2);
	modelview.at<precision>(1, 2) = rotation.at<precision>(1, 2);
	modelview.at<precision>(2, 2) = rotation.at<precision>(2, 2);
	modelview.at<precision>(3, 2) = 0;

	modelview.at<precision>(0, 3) = translation.at<precision>(0, 0);
	modelview.at<precision>(1, 3) = translation.at<precision>(1, 0);
	modelview.at<precision>(2, 3) = translation.at<precision>(2, 0);
	modelview.at<precision>(3, 3) = 1;

	// This matrix corresponds to the change of coordinate systems.
	static double changeCoordArray[4][4] = { { 1, 0, 0, 0 },{ 0, -1, 0, 0 },{ 0, 0, -1, 0 },{ 0, 0, 0, 1 } };
	static Mat changeCoord(4, 4, CV_64FC1, changeCoordArray);

	modelview = changeCoord*modelview;
}

// Renders background using opengl (draw opencv frame as opengl texture)
void renderBackgroundGL(const Mat *image)
{
	GLint polygonMode[2];
	glGetIntegerv(GL_POLYGON_MODE, polygonMode);
	glPolygonMode(GL_FRONT, GL_FILL);
	glPolygonMode(GL_BACK, GL_FILL);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//gluOrtho2D(-1.0, 1.0, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	static bool textureGenerated = false;
	static GLuint textureId;
	if (!textureGenerated)
	{
		glGenTextures(1, &textureId);

		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		textureGenerated = true;
	}

	// Copy the image to the texture.
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->size().width, image->size().height, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, image->data);

	// Draw the image.
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glNormal3f(0.0, 0.0, 1.0);

	glTexCoord2f(0.0, 0.0);
	glVertex3f(-1.0, 1.0, 0.0);
	
	glTexCoord2f(1.0, 0.0);
	glVertex3f(1.0, 1.0, 0.0);
	
	glTexCoord2f(1.0, 1.0);
	glVertex3f(1.0, -1.0, 0.0);
	
	glTexCoord2f(0.0, 1.0);
	glVertex3f(-1.0, -1.0, 0.0);

	glEnd();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LIGHTING);

	// Clear the depth buffer so the texture forms the background.
	glClear(GL_DEPTH_BUFFER_BIT);

	// Restore the polygon mode state.
	glPolygonMode(GL_FRONT, polygonMode[0]);
	glPolygonMode(GL_BACK, polygonMode[1]);
}

// Draw animating cube. Scale is changed such that the cube pulses over time.
void drawObject(const Mat *projection_mat, const Mat *modelview_mat, double t = 0)
{
	// Change the scale of the cube over time
	float scale = 10.0 + 3 * sin(t * 2);
	
	//glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	GLfloat* projection = convertMatrixType(*projection_mat);
	glLoadMatrixf(projection);
	delete[] projection;
	glMatrixMode(GL_MODELVIEW);
	GLfloat* modelView = convertMatrixType(*modelview_mat);
//	glPushMatrix();
	glLoadMatrixf(modelView);
	delete[] modelView;

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glTranslatef(15.0, 15.0, 0.0);
	glScalef(scale, scale, scale);
	glRotatef(180, 0, 1, 0);
	glTranslatef(0, 0, 0.5);
	glColor3f(1.0, 0.0, 0.0);
	//White side - FRONT
	glBegin(GL_QUADS);

	glColor3f(1.0, 1.0, 1.0);
	glVertex3f(0.5, -0.5, -0.5);
	glVertex3f(0.5, 0.5, -0.5);
	glVertex3f(-0.5, 0.5, -0.5);
	glVertex3f(-0.5, -0.5, -0.5);

	glEnd();
	// Yellow side - BACK
	glBegin(GL_QUADS);
	glColor3f(1.0, 1.0, 0.0);
	glVertex3f(0.5, -0.5, 0.5);
	glVertex3f(0.5, 0.5, 0.5);
	glVertex3f(-0.5, 0.5, 0.5);
	glVertex3f(-0.5, -0.5, 0.5);
	glEnd();

	// Purple side - RIGHT
	glBegin(GL_QUADS);
	glColor3f(1.0, 0.0, 1.0);
	glVertex3f(0.5, -0.5, -0.5);
	glVertex3f(0.5, 0.5, -0.5);
	glVertex3f(0.5, 0.5, 0.5);
	glVertex3f(0.5, -0.5, 0.5);
	glEnd();

	// Green side - LEFT
	glBegin(GL_QUADS);
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(-0.5, -0.5, 0.5);
	glVertex3f(-0.5, 0.5, 0.5);
	glVertex3f(-0.5, 0.5, -0.5);
	glVertex3f(-0.5, -0.5, -0.5);
	glEnd();

	// Blue side - TOP
	glBegin(GL_QUADS);
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(0.5, 0.5, 0.5);
	glVertex3f(0.5, 0.5, -0.5);
	glVertex3f(-0.5, 0.5, -0.5);
	glVertex3f(-0.5, 0.5, 0.5);
	glEnd();

	// Red side - BOTTOM
	glBegin(GL_QUADS);
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.5, -0.5, -0.5);
	glVertex3f(0.5, -0.5, 0.5);
	glVertex3f(-0.5, -0.5, 0.5);
	glVertex3f(-0.5, -0.5, -0.5);
	glEnd();
	//glutWireTeapot(10.0 / curr);
	glEnable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
//	glPopMatrix();
	glColor3f(1.0, 1.0, 1.0);

}

// Callback function called by updatewindow().
// Draw background and cube, then save rendered data into outImg.
void on_opengl(void *param)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	contextdata *context = (contextdata *)param;
	// draw background
	renderBackgroundGL(context->frame);
	// draw object
	if (context->drawObject)
	{
		//cout << *(context->modelview) << endl;
		double t = (double)(getTickCount() - context->startTick) / getTickFrequency();
		drawObject(context->projection, context->modelview, t);
	}
	glFlush();
	glReadPixels(0, 0, context->frame->size().width, context->frame->size().width, GL_BGR_EXT, GL_UNSIGNED_BYTE, context->outImg->data);
}


int main(int, char**)
{
	// Read camera calibration data
	Mat camera_mat;
	Mat distortion_coeff;
	float sqr_size;
	int board_width, board_height;

    //  Read camera’s intrinsic matrix
    FileStorage fs("camera_data.xml", FileStorage::READ);
	fs["camera_matrix"] >> camera_mat;
	fs["distortion_coefficients"] >> distortion_coeff;
	fs["square_size"] >> sqr_size;
	fs["board_width"] >> board_width;
	fs["board_height"] >> board_height;

	fs.release();

	Size board_size(board_width, board_height); //interior number of corners
	// initialize corners in 3D
	vector<Point3f> corners_3d;
	for (int i = 0; i < board_size.height; ++i)
		for (int j = 0; j < board_size.width; ++j)
			corners_3d.push_back(Point3f(j * sqr_size, i * sqr_size, 0));

	vector<Point3f> axis = { {10.0, 10.0, 0.0}, { 25.0, 10.0, 0.0 }, { 10.0, 25.0, 0.0 }, { 10.0, 10.0, -15.0 } };

    // Open video or camera
	VideoCapture inputvideo(0); // open the default camera
	if (!inputvideo.isOpened())  // check if we succeeded
		return -1;

    // Create image writer
    int frame_width = inputvideo.get(CV_CAP_PROP_FRAME_WIDTH);
    int frame_height = inputvideo.get(CV_CAP_PROP_FRAME_HEIGHT);
    VideoWriter video("out.avi", CV_FOURCC('X', 'V', 'I', 'D'), 15, Size(frame_width, frame_height), true);
    //VideoWriter video("out.avi", CV_FOURCC('M', 'J', 'P', 'G'), 15, Size(frame_width, frame_height), true);
    
    // Window "edges" shows the rendering result, and window "edges1" shows the output result
    // edges is the OpenGL result and edges1 is converted back to the OpenCV image because we cannot directly 
    // output the OpenGL result to video.
	Mat gray;
    namedWindow("edges", WINDOW_OPENGL + WINDOW_AUTOSIZE);
    //namedWindow("edges", WINDOW_OPENGL);
    resizeWindow("edges", frame_width, frame_height);
    namedWindow("edges1");
	
    Mat frame;
	Mat modelview(4, 4, CV_64FC1);
	Mat Projection(4, 4, CV_64FC1);
	bool correspondence;
	Mat glimg(Size(frame_width, frame_height), CV_8UC3);
	Mat out_img(Size(frame_width, frame_height), CV_8UC3);
	contextdata context;
	context.frame = &frame;
	context.outImg = &glimg;
	context.projection = &Projection;
	context.drawObject = false;
	context.modelview = &modelview;
	context.startTick = getTickCount();

	setOpenGlDrawCallback("edges", on_opengl, (void *)&context);
	for (;;)
	{
		inputvideo >> frame; // get a new frame from camera
		if (frame.empty())
			break;
		cvtColor(frame, gray, COLOR_BGR2GRAY);
		
		vector<Point2f> corners; // this will be filled by the detected corners

		//CALIB_CB_FAST_CHECK is faster on images not containing any chessboard corners
		bool patternfound = findChessboardCorners(gray, board_size, corners,
			CALIB_CB_ADAPTIVE_THRESH + CALIB_CB_FAST_CHECK);
		context.drawObject = false;
		if (patternfound)
		{
            // Refine corners
            cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
				TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));
			//drawChessboardCorners(gray, board_size, Mat(corners), patternfound);
		
			// get pose of chessboard
			Mat rvec = Mat::zeros(3, 1, CV_64FC1);
			Mat tvec = Mat::zeros(3, 1, CV_64FC1);
			vector<Point2f> imagePoints;
            // Compute intrinsic and extrinsic matrices of camera
            correspondence = solvePnP(corners_3d, corners, camera_mat, distortion_coeff, rvec, tvec);
			Mat rotation;
			Rodrigues(rvec, rotation);
			if (correspondence) {  // Generate intrinsic and extrinsic matrices of camera for OpenGL
				generateProjectionModelview(camera_mat, rotation, tvec, Projection, modelview);
				/*
				projectPoints(axis, rvec, tvec, camera_mat, distortion_coeff, imagePoints);
				line(frame, imagePoints[0], imagePoints[1], CV_RGB(255, 0, 0), 3);
				line(frame, imagePoints[0], imagePoints[2], CV_RGB(0, 255, 255), 3);
				line(frame, imagePoints[0], imagePoints[3], CV_RGB(0, 0, 255), 3);
				*/
				context.drawObject = true;
			}
		}
		//loadTexture(frame);
		updateWindow("edges"); // Update window to invoke on_opengl()

        // Show and write rendering result
		//angle += 4;
		flip(glimg, out_img, 0);
		imshow("edges1", out_img);
		video.write(out_img);
		if (waitKey(30) >= 0) break;
	}
	// the camera will be deinitialized automatically in VideoCapture destructor
	video.release();
	inputvideo.release();
	destroyAllWindows();
	return 0;
}
