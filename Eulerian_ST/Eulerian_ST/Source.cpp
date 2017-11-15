//#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
//#define __CL_ENABLE_EXCEPTIONS
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>
#include <opencv2/opencv.hpp>

#ifdef __APPLE__
//include <OpenCL/cl.hpp>
#else
//#include <CL/cl.hpp>
#endif

//#include "Utils.h"

using namespace std;
using namespace cv;
//using namespace cl;

class Speckle_Results 
{
public:
	char * Image;
	int Frame_No;
};

class uFileHeader
{
public:
	/// data type - data types can also be determined by file extensions
	int type;
	/// number of frames in file
	int frames;
	/// width - number of vectors for raw data, image width for processed data
	int w;
	/// height - number of samples for raw data, image height for processed data
	int h;
	/// data sample size in bits
	int ss;
	/// roi - upper left (x)
	int ulx;
	/// roi - upper left (y)
	int uly;
	/// roi - upper right (x)
	int urx;
	/// roi - upper right (y)
	int ury;
	/// roi - bottom right (x)
	int brx;
	/// roi - bottom right (y)
	int bry;
	/// roi - bottom left (x)
	int blx;
	/// roi - bottom left (y)
	int bly;
	/// probe identifier - additional probe information can be found using this id
	int probe;
	/// transmit frequency
	int txf;
	/// sampling frequency
	int sf;
	/// data rate - frame rate or pulse repetition period in Doppler modes
	int dr;
	/// line density - can be used to calculate element spacing if pitch and native # elements is known
	int ld;
	/// extra information - ensemble for color RF
	int extra;
};

void InitialiseSpeckleResults(Speckle_Results sr[], int lengthFrames, int size_Sonix);
void DeleteSpeckleResults(Speckle_Results sr[], int lengthFrames);
void DisplayImage(int fr, int width, int height, Speckle_Results *sr, string Title);
FILE* ReadFile();
uFileHeader ReadHeader(FILE *fp);
Speckle_Results* ReadImageData(FILE *fp, uFileHeader hdr);
Mat convertMat(int hdr_width, int hdr_height, int fr, Speckle_Results *sr);
void BlockMatching(uFileHeader hdr, Speckle_Results *sr);
Vec3i Closest(const Mat& referenceFrame, const Point& currentPoint, const int N, const int width, const int height);
void BlockMatchingSAD(Mat& currentFrame, Mat& referenceFrame, Point * &motion, Point2f * &details, int N, int stepSize, int width, int height, int blocksW, int blocksH);

int main(int argc, char **argv) {
	FILE *fp = ReadFile();
	uFileHeader hdr = ReadHeader(fp);
	Speckle_Results *sr = ReadImageData(fp, hdr);

	//DisplayImage(0, hdr.w, hdr.h, sr);
	/*
	Mat ImageRaw = convertMat(hdr.w, hdr.h, 0, sr);
	for (int x = 0; x < hdr.h; x++) {
		for (int y = 0; y < hdr.w; y++) {
			double iout = (double)ImageRaw.at<uchar>(x, y);

			if (iout == 255) {
				cout << x << ", " << y << endl;
			}
		}
	}
	*/

	BlockMatching(hdr, sr);
	destroyAllWindows();
	system("pause");
	exit(-1);
	return 0;
}

void InitialiseSpeckleResults(Speckle_Results sr[], int lengthFrames, int size_Sonix) {
	for (int i = 0; i < lengthFrames; i++)
	{
		sr[i].Image = new char[size_Sonix];
	}
}

void DeleteSpeckleResults(Speckle_Results sr[], int lengthFrames) {
	for (int i = 0; i < lengthFrames; i++)
	{
		delete[] sr[i].Image;
	}
}

void DisplayImage(int fr, int width, int height, Speckle_Results *sr, string Title) {
	Mat ImageRaw = Mat(cvSize(width, height), CV_8UC1);
	for (int pixelp = 0; pixelp < width * height; pixelp++) {
		ImageRaw.data[pixelp] = sr[fr].Image[pixelp];
	}
	namedWindow(Title, WINDOW_AUTOSIZE);
	imshow(Title, ImageRaw);
	waitKey(1);
}

FILE* ReadFile() {
	// direc = Documents/InOut/FILENAME
	CHAR Direc[MAX_PATH];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, Direc);
	char Name[30];
	cout << endl;
	cout << "Enter file name: ";
	cin >> Name;
	strcat(Direc, "/InOut/");
	strcat(Direc, Name);
	cout << endl;
	cout << "Reading data from hard disk..." << endl;
	cout << endl;
	// open data file
	FILE * fp = fopen(Direc, "rb");
	if (!fp) {
		printf("Error opening input file from %s\n", Direc);
		cout << endl;
		system("pause");
		exit(-1);
	}

	return fp;
}

uFileHeader ReadHeader(FILE *fp) {
	// read header
	uFileHeader hdr;
	fread(&hdr, sizeof(hdr), 1, fp);
	int size_hdr = sizeof(hdr);
	printf("image width: %d\n", hdr.w);
	printf("image height: %d\n", hdr.h);
	printf("image (data) sample size: %d\n", hdr.ss);
	printf("frames in file: %d\n", hdr.frames);
	printf("frames rate: %d\n", hdr.dr);
	cout << endl;

	return hdr;
}

Speckle_Results* ReadImageData(FILE *fp, uFileHeader hdr) {
	int hdr_frames = hdr.frames;
	int FrameRate = hdr.dr;
	int size_Sonix = 0;
	Mat ImageRaw;
	Speckle_Results * sr = new Speckle_Results[hdr_frames];

	// print file information
	printf("file details:\n");

	switch (hdr.type)
	{
	case 4:
		printf("type: post-scan B-mode data (8-bit)  *.b8\n");
		unsigned char * data;
		size_Sonix = hdr.w * hdr.h * (hdr.ss / 8);
		data = new unsigned char[size_Sonix];

		InitialiseSpeckleResults(sr, hdr_frames, size_Sonix);

		// read data
		for (int fr = 0; fr < hdr.frames; fr++) {
			fread(data, size_Sonix, 1, fp); // read from stream
			memcpy(sr[fr].Image, data, size_Sonix); // copy current data memory to array
			//DisplayImage(fr, hdr.w, hdr.h, sr, "Image_Raw"); //display image frame
		}
		delete[] data;
		break;

	case 16:
		printf("type: RF data (16-bit)  *.rf\n");
		short * data2;   // also need to change gBuffer in Types.h
		size_Sonix = hdr.w * hdr.h * (hdr.ss / 8);
		data2 = new short[size_Sonix];

		// read data
		for (int fr = 0; fr < hdr.frames; fr++) {
			fread(data2, size_Sonix, 1, fp);
		}
		delete[] data2;
		break;
	}

	fclose(fp);
	
	cout << endl;
	cout << "Finished reading data from hard disk...\n" << endl;
	return sr;
}

Mat convertMat(int hdr_width, int hdr_height, int fr, Speckle_Results *sr) {
	//convert *sr to Mat
	Mat ImageRaw = Mat(cvSize(hdr_width, hdr_height), CV_8UC1);
	for (int pixelp = 0; pixelp < hdr_width * hdr_height; pixelp++) {
		ImageRaw.data[pixelp] = sr[fr].Image[pixelp];
	}
	return ImageRaw;
}

void BlockMatching(uFileHeader hdr, Speckle_Results *sr) {
	int N = 40; //block size
	int M = 10; //search window

	int blocksH = ceil(hdr.h / N);
	int blocksW = ceil(hdr.w / N);
	Mat currentFrame;
	Mat referenceFrame;
	//point array for storing motion
	Point * motion = new Point[blocksH * blocksW];
	Point2f * details = new Point2f[blocksH * blocksW];
	//Create Real-Time graph to display average angular motion
	//SimpleGraph motion_graph(1024, 512, 128);

	string window = "BM";
	namedWindow(window, WINDOW_AUTOSIZE);

	//loop through frames
	for (int fr = 1; fr < hdr.frames - 1; fr++) {
		//forwards prediction
		referenceFrame = convertMat(hdr.w, hdr.h, fr - 1, sr);
		currentFrame = convertMat(hdr.w, hdr.h, fr, sr);
		BlockMatchingSAD(referenceFrame, currentFrame, motion, details, N, M, hdr.w, hdr.h, blocksW, blocksH);
		for (int i = 0; i < (blocksH * blocksW); i++){
			line(referenceFrame, details[i], details[i-1], Scalar(255, 255, 0), 5, 8, 0);
		}
		imshow(window, referenceFrame);
		waitKey(1);
	}
}

Vec3i Closest(const Mat& referenceFrame, const Point& currentPoint, const int N, const int width, const int height) {
	for (int r = -N; r < N; r += N) {
		for (int c = -N; c < N; c += N) {
			Point referencePoint(currentPoint.x + r, currentPoint.y + c);
			//if search area within bounds
			if (referencePoint.y >= 0 && referencePoint.y < height - N && referencePoint.x >= 0 && referencePoint.x < width - N) {
				return Vec3i(r, c, sum(abs(referenceFrame(Rect(referencePoint.x, referencePoint.y, N, N))))[0]);
			}
		}
	}
	Point referencePoint(currentPoint.x, currentPoint.y);
	return Vec3i(0, 0, sum(abs(referenceFrame(Rect(referencePoint.x, referencePoint.y, N, N))))[0]);
}

void BlockMatchingSAD(Mat& currentFrame, Mat& referenceFrame, Point * &motion, Point2f * &details, int N, int M, int width, int height, int blocksW, int blocksH) {
	//for all blocks in frame
	for (int x = 0; x < blocksW; x++) {
		for (int y = 0; y < blocksH; y++) {
			//current frame reference to be searched for in previous frame
			const Point currentPoint(x * M, y * M);
			int idx = x + y * blocksW;
			Vec3i closestPoint = Closest(referenceFrame, currentPoint, N, width, height);
			int lowestSSD = INT_MAX;
			int SSD = 0;
			float blockDistance = FLT_MAX;
			Point referencePoint(currentPoint.x, currentPoint.y);

			//Loop over all possible blocks within each macroblock
			for (int row = closestPoint[0]; row < N; row++) {
				for (int col = closestPoint[1]; col < N; col++) {
					//Refererence a block to search on the previous frame
					Point referencePoint(currentPoint.x + row, currentPoint.y + col);

					//Check if it lays within the bounds of the capture
					if (referencePoint.y >= 0 && referencePoint.y < height - N && referencePoint.x >= 0 && referencePoint.x < width - N) {
						//Calculate SSD
						SSD = sum(abs(currentFrame(Rect(currentPoint.x, currentPoint.y, N, N)) - referenceFrame(Rect(referencePoint.x, referencePoint.y, N, N))))[0];

						//Take the lowest error
						float distance = sqrt((float)(((referencePoint.x - currentPoint.x) * (referencePoint.x - currentPoint.x)) + ((referencePoint.y - currentPoint.y) * (referencePoint.y - currentPoint.y))));

						//Write buffer with the lowest error
						if (SSD < lowestSSD || (SSD == lowestSSD && distance <= blockDistance)) {
							lowestSSD = SSD;
							blockDistance = distance;
							float p0x = currentPoint.x, p0y = currentPoint.y - sqrt((float)(((referencePoint.x - p0x) * (referencePoint.x - p0x)) + ((referencePoint.y - currentPoint.y) * (referencePoint.y - currentPoint.y))));
							float angle = (2 * atan2(referencePoint.y - p0y, referencePoint.x - p0x)) * 180 / M_PI;
							motion[idx] = referencePoint;
							details[idx] = Point2f(angle, blockDistance);
						}
					}
				}
			}
		}
	}
}