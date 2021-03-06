#include "stdafx.h"
#include "common.h"
#include <stdlib.h>
#include <random>
#include "queue"
#include <algorithm>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
using namespace cv;

// Display window
const char* WIN_SOURCE = "Source"; //window for the source frame
const Scalar RED = Scalar(0, 0, 255);
const Scalar GREEN = Scalar(0, 255, 0);
const Scalar BLUE = Scalar(255, 0, 0);
const Scalar WHITE = Scalar(255, 255, 255);

int parking_lot_config_nr = 0;
int parking_lots_number = 0;
int parking_lots_free = 0;

double video_fps = 0;

Mat frame;
vector<Point> settingPointsVector;
vector<vector<Point>> stack;

const int DENSITY_THRESHOLD = 10;

struct parking {
	vector < Point > rect;
	int state;
	int parking_number;
	float edge_density;
	int colorRange_A;
	int colorRange_B;
	int colorTotal_A;
	int colorTotal_B;
};

vector<parking> allParkings;


Mat Dilation(Mat img){
	int times = 0;
	int dx7[] = { -1, 1, 0, 0 };
	int dy7[] = { 0, 0, -1, 1 };

	for (int tt = 0; tt < times; tt++){

		Mat newImg(img.rows, img.cols, CV_8UC1, Scalar(0));
		for (int i = 1; i < img.rows - 1; i++){
			for (int j = 1; j < img.cols - 1; j++){
				if (img.at<uchar>(i, j) == 255){
					for (int contor = 0; contor < 4; contor++){
						newImg.at<uchar>(i + dx7[contor], j + dy7[contor]) = 255;
						newImg.at<uchar>(i, j) = 255;
					}
				}
			}
		}

		img = newImg;
	}
	return img;
}

Mat L10_gaussian_filter_1x2d(Mat img){
	float w = 7;
	float sigma = w / 6;

	Mat newImg = Mat::zeros(img.rows, img.cols, CV_8UC1);
	Mat conv = Mat(w, w, CV_32FC1);

	float firstValue = 1 / (PI * 2 * (sigma *sigma));
	float sumaConv = 0;

	float x0 = w / 2;
	float y0 = w / 2;
	for (int i = 0; i < w; i++){
		for (int j = 0; j < w; j++){
			conv.at<float>(i, j) = firstValue * exp(-1 * ((i - x0)*(i - x0) + (j - y0)*(j - y0)) / (sigma *sigma * 2));
			sumaConv += conv.at<float>(i, j);
		}
	}

	double t = (double)getTickCount();

	int size = w;
	float normalize = sumaConv;
	int draw = size / 2;
	int dcol = size / 2;

	for (int i = draw; i < img.rows - draw; i++){
		for (int j = dcol; j < img.cols - dcol; j++){
			float value = 0;
			for (int r = 0; r < size; r++){
				for (int c = 0; c < size; c++){
					value += img.at<uchar>(r - draw + i, c - dcol + j) * conv.at<float>(r, c);
				}
			}
			value /= normalize;
			newImg.at<uchar>(i, j) = value;
		}
	}

	t = ((double)getTickCount() - t) / getTickFrequency();

	return newImg;
}

void DefineParkingSpot(int event, int x, int y, int flags, void* ptr)
{
	Point p;

	if (event == CV_EVENT_LBUTTONDOWN)
	{
		if (settingPointsVector.size() != 3){
			p.x = x;
			p.y = y;
			settingPointsVector.push_back(p);
			circle(frame, p, 2, BLUE, 2, 8, 0);
			imshow(WIN_SOURCE, frame);
		}
		else {
			allParkings.clear();
			p.x = x;
			p.y = y;
			settingPointsVector.push_back(p);
			stack.push_back(settingPointsVector);
			for (std::vector <vector<Point >> ::iterator it = stack.begin(); it != stack.end(); ++it) {
				vector < Point > rect = *it;
				Point poly[1][4];
				poly[0][0] = rect[0];
				poly[0][1] = rect[1];
				poly[0][2] = rect[2];
				poly[0][3] = rect[3];
				const Point* ppt[1] = { poly[0] };
				int npt[] = { 4 };
				fillPoly(frame, ppt, npt, 1, Scalar(255, 255, 255), 8);
				imshow(WIN_SOURCE, frame);
				parking park;
				park.rect = rect;
				park.state = 0;
				++parking_lot_config_nr;
				park.parking_number = parking_lot_config_nr;
				allParkings.push_back(park);
			}
			settingPointsVector.clear();
		}
	}
}

void getSettingPoints(Mat frame) {
	for each (Point point in settingPointsVector)
	{
		circle(frame, point, 2, BLUE, 2, 8, 0);
	}
}

void edgeDetection(Mat gray,  Mat grayLeveled, Mat output, Mat output2, Mat dilated) {
	int low_threshold = 30;
	const char* edges_detected = "Edges detected";
	namedWindow(edges_detected, CV_WINDOW_AUTOSIZE);

	cvtColor(frame, gray, CV_BGR2GRAY);
	grayLeveled = L10_gaussian_filter_1x2d(gray);
	Canny(grayLeveled, dilated, low_threshold, 3 * low_threshold, 3);
	output = Dilation(dilated);
	frame.copyTo(output2);
	parking_lots_free = 0;

	getSettingPoints(frame);

	for (std::vector <parking> ::iterator p = allParkings.begin(); p != allParkings.end(); ++p) {
		Scalar color;

		if (p->state == 0){
			color = GREEN;
			parking_lots_free++;
		}
		else {
			color = RED;
		}

		float minY = min(min(p->rect[0].y, p->rect[1].y), min(p->rect[2].y, p->rect[3].y));
		float minX = min(min(p->rect[0].x, p->rect[1].x), min(p->rect[2].x, p->rect[3].x));
		float maxY = max(max(p->rect[0].y, p->rect[1].y), max(p->rect[2].y, p->rect[3].y));
		float maxX = max(max(p->rect[0].x, p->rect[1].x), max(p->rect[2].x, p->rect[3].x));
		rectangle(frame, Point(minX, minY), Point(maxX, maxY), RED, 1, 8);

		Point poly[1][4];
		poly[0][0] = p->rect[0];
		poly[0][1] = p->rect[1];
		poly[0][2] = p->rect[2];
		poly[0][3] = p->rect[3];
		const Point* ppt[1] = { poly[0] };
		int npt[] = { 4 };

		fillPoly(output2, ppt, npt, 1, color, 8);
		double alpha = 0.3;
		cv::addWeighted(output2, alpha, frame, 1 - alpha, 0, frame);

		int totalPixels = 0;
		int whitePixels = 0;
		for (int y = minY + 1; y < maxY; y++){
			for (int x = minX + 1; x < maxX; x++){
				if (output2.at<Vec3b>(y, x) == Vec3b(0, 255, 0) || output2.at<Vec3b>(y, x) == Vec3b(0, 0, 255)){
					totalPixels++;
					if (output.at<uchar>(y, x) == 255){
						whitePixels++;
					}
				}

			}
		}

		float density = (whitePixels * 100) / totalPixels;
		std::cout << density << " dense \n";
		if (density > DENSITY_THRESHOLD){
			p->state = 1;
		}
		else {
			p->state = 0;
		}
	}

	imshow(WIN_SOURCE, frame);
	imshow(edges_detected, output);

	std::cout << parking_lots_free << "\n";
}

void colorDensityDetection(){
	Mat frameNoLum;
	cvtColor(frame, frameNoLum, CV_BGR2Lab);
	for (std::vector <parking> ::iterator p = allParkings.begin(); p != allParkings.end(); ++p) {
		float minY = min(min(p->rect[0].y, p->rect[1].y), min(p->rect[2].y, p->rect[3].y));
		float minX = min(min(p->rect[0].x, p->rect[1].x), min(p->rect[2].x, p->rect[3].x));
		float maxY = max(max(p->rect[0].y, p->rect[1].y), max(p->rect[2].y, p->rect[3].y));
		float maxX = max(max(p->rect[0].x, p->rect[1].x), max(p->rect[2].x, p->rect[3].x));

		Mat localFrame;
		frame.copyTo(localFrame);
		Point poly[1][4];
		poly[0][0] = p->rect[0];
		poly[0][1] = p->rect[1];
		poly[0][2] = p->rect[2];
		poly[0][3] = p->rect[3];
		const Point* ppt[1] = { poly[0] };
		int npt[] = { 4 };

		int hist_w = 256; int hist_h = 400;
		Mat a_hist(hist_h, hist_w, CV_8UC1, Scalar(0));
		Mat b_hist(hist_h, hist_w, CV_8UC1, Scalar(0));
		fillPoly(localFrame, ppt, npt, 1, WHITE, 8);
		for (int y = minY ; y < maxY; y++){
			for (int x = minX ; x < maxX; x++){
				Vec3b localPoint = localFrame.at<Vec3b>(y, x);
				if (localPoint == Vec3b(255, 255, 255)){
					int a_val = (int)frameNoLum.at<Vec3b>(y, x)[1];
					int b_val = (int)frameNoLum.at<Vec3b>(y, x)[2];
					a_hist.at<uchar>(0, a_val)++;
					b_hist.at<uchar>(0, b_val)++;
				}
			}
		} 

		int rangeThreshold = 10;
		int firstDetectedA, firstDetectedB, lastDetectedA, lastDetectedB, rangeA, rangeB;
		bool firstASet = false;
		bool firstBSet = false;

		int overstepValuesA = 0;
		int overstepValuesB = 0;

		for (int i = 0; i <= 255; i++) {
			int computed_value = a_hist.at<uchar>(0, i);
			if (computed_value >= rangeThreshold) {
				if (!firstASet) {
					firstASet = true;
					firstDetectedA = i;
				}
				lastDetectedA = i;
				overstepValuesA += computed_value;
			}
			a_hist.at<uchar>(computed_value, i) = 255;
			computed_value = b_hist.at<uchar>(0, i);
			if (computed_value >= rangeThreshold) {
				if (!firstBSet) {
					firstBSet = true;
					firstDetectedB = i;
				}
				lastDetectedB = i;
				overstepValuesB += computed_value;
			}
			b_hist.at<uchar>(computed_value, i) = 255;
		}
		rangeA = lastDetectedA - firstDetectedA;
		p->colorRange_A = rangeA;
		p->colorTotal_A = overstepValuesA;
		rangeB = lastDetectedB - firstDetectedB;
		p->colorRange_B = rangeB;
		p->colorTotal_B = overstepValuesB;
		std::cout << "Range A: " << rangeA << "       Range B: " << rangeB << "    Values A: " << overstepValuesA << "     Values B:" << overstepValuesB << "\n";
		imshow("a_hist", a_hist);
		imshow("b_hist", b_hist);
	}
}

void distanceTransform() {
	Mat kernel = (Mat_<float>(3, 3) <<
		1, 1, 1,
		1, -12, 1,
		1, 1, 1); 
	Mat imgLaplacian;
	Mat sharp = frame; 
	filter2D(sharp, imgLaplacian, CV_32F, kernel);
	frame.convertTo(sharp, CV_32F);
	Mat imgResult = sharp - imgLaplacian;
	imgResult.convertTo(imgResult, CV_8UC3);
	imgLaplacian.convertTo(imgLaplacian, CV_8UC3);
	imshow("New Sharped Image", imgResult);
	Mat bw;
	cvtColor(imgResult, bw, CV_BGR2GRAY);
	threshold(bw, bw, 40, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
	imshow("Binary Image", bw);

}

void parkLotDetect(VideoCapture cap) {
	Mat gray, output, output2, grayLeveled, dilated;

	// video resolution
	Size capS = Size((int)cap.get(CV_CAP_PROP_FRAME_WIDTH),
		(int)cap.get(CV_CAP_PROP_FRAME_HEIGHT));

	namedWindow(WIN_SOURCE, CV_WINDOW_AUTOSIZE);
	cvMoveWindow(WIN_SOURCE, 0, 0);

	char c;
	int count = 0;
	Scalar red = Scalar(0, 0, 255);
	Scalar green = Scalar(0, 255, 0);

	Point p;
	cap >> frame; // get a new frame from camera
	imshow(WIN_SOURCE, frame);
	setMouseCallback("Source", DefineParkingSpot, &p);
	parking_lots_number = allParkings.size();

	while (cap.isOpened()){
		cap.read(frame);
	
		if (frame.empty())
		{
			printf("End of the video file\n");
			break;
		}

		//edgeDetection(gray, grayLeveled, output, output2, dilated);
		//colorDensityDetection();
		distanceTransform();
		getSettingPoints(frame);
		imshow(WIN_SOURCE, frame);

		int wait_frame_second = 60 / (int)video_fps * 10;
		c = cvWaitKey(wait_frame_second);  // waits a key press to advance to the next frame
		if (c == 27) {
			// press ESC to exit
			printf("ESC pressed - capture finished");
			break;  //ESC pressed
		}
	}
}


int main()
{
	VideoCapture cap("Videos/park.mp4"); // open the default camera
	//VideoCapture cap(1); // open the default camera
	video_fps = cap.get(CV_CAP_PROP_FPS);
	if (!cap.isOpened())  // check if we succeeded
		return -1;

	parkLotDetect(cap);


	return 0;
}


//int thresh = 100;
//int max_thresh = 150;
///** @function cornerHarris_demo */
//void cornerHarris_demo(int, void*, Mat src)
//{
//	char* corners_window = "Corners detected";
//	namedWindow(corners_window, CV_WINDOW_AUTOSIZE);
//
//	Mat dst, dst_norm, dst_norm_scaled, src_gray;
//	dst = Mat::zeros(src.size(), CV_32FC1);
//	cvtColor(src, src_gray, CV_BGR2GRAY);
//
//	Mat src_bw;
//	cv::Mat src_bw = img > 128;
//
//	imshow("TESTING", src_gray);
//	/// Detector parameters
//	int blockSize = 2;
//	int apertureSize = 3;
//	double k = 0.07;
//
//	/// Detecting corners
//	cornerHarris(src_gray, dst, blockSize, apertureSize, k, BORDER_CONSTANT);
//
//	/// Normalizing
//	normalize(dst, dst_norm, 0, 255, NORM_MINMAX, CV_32FC1, Mat());
//	convertScaleAbs(dst_norm, dst_norm_scaled);
//
//	/// Drawing a circle around corners
//	for (int j = 0; j < dst_norm.rows; j++)
//	{
//		for (int i = 0; i < dst_norm.cols; i++)
//		{
//			if ((int)dst_norm.at<float>(j, i) > thresh)
//			{
//				circle(src, Point(i, j), 2, RED, 1, 8, 0);
//			}
//		}
//	}
//	/// Showing the result
//	namedWindow(corners_window, CV_WINDOW_AUTOSIZE);
//	imshow(corners_window, src);
//}

