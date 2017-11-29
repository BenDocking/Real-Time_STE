#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define __CL_ENABLE_EXCEPTIONS
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <math.h>
#include <opencv2/opencv.hpp>
#include <CL/cl.hpp>

//#include "Utils.h"

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
void DisplayImage(int fr, int width, int height, Speckle_Results *sr, std::string Title);
FILE* ReadFile();
uFileHeader ReadHeader(FILE *fp);
Speckle_Results* ReadImageData(FILE *fp, uFileHeader hdr);
cv::Mat convertMat(int hdr_width, int hdr_height, int fr, Speckle_Results *sr);
void BlockMatching(uFileHeader hdr, Speckle_Results *sr);
cv::Vec3i Closest(const cv::Mat& referenceFrame, const cv::Point& currentPoint, const int searchWindow, const int width, const int height, const int N);
void BlockMatchingFrame(cv::Mat& currentFrame, cv::Mat& referenceFrame, cv::Point * &motion, cv::Point2f * &details, int N, int stepSize, int width, int height, int blocksW, int blocksH, int similarityMeasure);

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
	cv::destroyAllWindows();
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

void DisplayImage(int fr, int width, int height, Speckle_Results *sr, std::string Title) {
	cv::Mat ImageRaw = cv::Mat(cvSize(width, height), CV_8UC1);
	for (int pixelp = 0; pixelp < width * height; pixelp++) {
		ImageRaw.data[pixelp] = sr[fr].Image[pixelp];
	}
	cv::namedWindow(Title, cv::WINDOW_AUTOSIZE);
	imshow(Title, ImageRaw);
	cv::waitKey(1);
}

FILE* ReadFile() {
	// direc = Documents/InOut/FILENAME
	CHAR Direc[MAX_PATH];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, Direc);
	char Name[30];
	std::cout << std::endl;
	std::cout << "Enter file name: ";
	std::cin >> Name;
	strcat(Direc, "/InOut/");
	strcat(Direc, Name);
	std::cout << std::endl;
	std::cout << "Reading data from hard disk..." << std::endl;
	std::cout << std::endl;
	// open data file
	FILE * fp = fopen(Direc, "rb");
	if (!fp) {
		printf("Error opening input file from %s\n", Direc);
		std::cout << std::endl;
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
	std::cout << std::endl;

	return hdr;
}

Speckle_Results* ReadImageData(FILE *fp, uFileHeader hdr) {
	int hdr_frames = hdr.frames;
	int FrameRate = hdr.dr;
	int size_Sonix = 0;
	bool lastFrame = false;
	cv::Mat ImageRaw;
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
			if (fr == hdr.frames - 1) {
				lastFrame = true;
			}
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
	
	std::cout << std::endl;
	std::cout << "Finished reading data from hard disk...\n" << std::endl;
	return sr;
}

cv::Mat convertMat(int hdr_width, int hdr_height, int fr, Speckle_Results *sr) {
	//convert *sr to Mat
	cv::Mat ImageRaw = cv::Mat(cvSize(hdr_width, hdr_height), CV_8UC1);
	for (int pixelp = 0; pixelp < hdr_width * hdr_height; pixelp++) {
		ImageRaw.data[pixelp] = sr[fr].Image[pixelp];
	}
	return ImageRaw;
}

void BlockMatching(uFileHeader hdr, Speckle_Results *sr) {
	int N = 10; //block size
	int stepSize = 5; //step size of blocks
	int blocksH = ceil(hdr.h / stepSize);
	int blocksW = ceil(hdr.w / stepSize);
	int similarityMeasure; //0 = SAD .. 1 = SSD 
	cv::Mat currentFrame;
	cv::Mat referenceFrame;
	//point array for outputs
	cv::Point * motion = new cv::Point[blocksH * blocksW];
	cv::Point2f * details = new cv::Point2f[blocksH * blocksW];

	std::cout << "Enter number corresponding to similarity measure:\n\n1. SAD\n2. SSD\n\n";
	std::cin >> similarityMeasure;

	std::string window = "BM";
	cv::namedWindow(window, cv::WINDOW_AUTOSIZE);

	//cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
	//checkErr(err, "Cannot create the command queue");

	//loop through frames
	for (int fr = 1; fr < hdr.frames; fr++) {
		//forwards prediction
		referenceFrame = convertMat(hdr.w, hdr.h, fr - 1, sr);
		currentFrame = convertMat(hdr.w, hdr.h, fr, sr);
		BlockMatchingFrame(referenceFrame, currentFrame, motion, details, N, stepSize, hdr.w, hdr.h, blocksW, blocksH, similarityMeasure);
		cv::Mat display;
		cvtColor(currentFrame, display, CV_GRAY2RGB);

		bool drawGrid = false;
		cv::Scalar rectColour = cv::Scalar(255);
		cv::Scalar lineColour = cv::Scalar(0, 255, 255);

		for (std::size_t i = 0; i < blocksW-1; i++)
		{
			for (std::size_t j = 0; j < blocksH-1; j++)
			{
				cv::Scalar intensity = currentFrame.at<uchar>(j * stepSize, i * stepSize);
				int iVal = intensity.val[0];
				if (iVal < 90) {
					lineColour = cv::Scalar(0, 255, 255);
				}
				else {
					lineColour = cv::Scalar(0, 0, 255);
				}
				if (iVal > 30) {
					//Calculate repective position of motion vector
					int idx = i + j * blocksW;

					//Offset drawn point to represent middle rather than top left of block
					cv::Point offset(N / 2, N / 2);
					cv::Point pos(i * stepSize, j * stepSize);
					cv::Point mVec(motion[idx].x, motion[idx].y);

					if (drawGrid) {
						rectangle(display, pos, pos + cv::Point(N, N), rectColour);
					}
					//only display motion vectors with motion
					if (details[idx].y != 0 || details[idx].y != 0) {
						arrowedLine(display, pos + offset, mVec + offset, lineColour);
					}
				}
			}
		}
		char frameInfo[200];
		sprintf(frameInfo, "Frame %d of %d", fr, hdr.frames);
		cv::putText(display, frameInfo, cvPoint(30, 30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(200, 200, 250), 1, CV_AA);
		imshow(window, display);
		cv::waitKey(1);
	}
}

cv::Vec3i Closest(const cv::Mat& referenceFrame, const cv::Point& currentPoint, const int searchWindow, const int width, const int height, const int N) {
	for (int r = -searchWindow; r < searchWindow; r += searchWindow) {
		for (int c = -searchWindow; c < searchWindow; c += searchWindow) {
			cv::Point referencePoint(currentPoint.x + r, currentPoint.y + c);
			//if search area within bounds
			if (referencePoint.y >= 0 && referencePoint.y < height - searchWindow && referencePoint.x >= 0 && referencePoint.x < width - searchWindow) {
				return cv::Vec3i(r, c, cv::sum(abs(referenceFrame(cv::Rect(referencePoint.x, referencePoint.y, N, N))))[0]);
			}
		}
	}
	cv::Point referencePoint(currentPoint.x, currentPoint.y);
	return cv::Vec3i(0, 0, cv::sum(abs(referenceFrame(cv::Rect(referencePoint.x, referencePoint.y, N, N))))[0]);
}

void BlockMatchingFrame(cv::Mat& currentFrame, cv::Mat& referenceFrame, cv::Point * &motion, cv::Point2f * &details, int N, int stepSize, int width, int height, int blocksW, int blocksH, int similarityMeasure) {
	//for all blocks in frame
	for (int x = 0; x < blocksW-1; x++) {
		for (int y = 0; y < blocksH-1; y++) {
			//current frame reference to be searched for in previous frame
			const cv::Point currentPoint(x * stepSize, y * stepSize);
			int idx = x + y * blocksW;
			//int M = 500;
			//Vec3i closestPoint = Closest(referenceFrame, currentPoint, M, width, height, N);
			int lowestSimilarity = INT_MAX;
			int similarity = 0;
			float blockDistance = FLT_MAX;
			cv::Point referencePoint(currentPoint.x, currentPoint.y);

			//loop over all posible blocks within search window/ all macroblocks
			for (int row = -10; row < 10; row++) {
				for (int col = -10; col < 10; col++) {
					//Refererence a block to search on the previous frame
					cv::Point referencePoint(currentPoint.x + row, currentPoint.y + col);

					//Check if it lays within the bounds of the capture
					if (referencePoint.y >= 0 && referencePoint.y < height - N && referencePoint.x >= 0 && referencePoint.x < width - N) {
						if (similarityMeasure == 0) {
							//Calculate SAD
							similarity = cv::sum(abs(currentFrame(cv::Rect(currentPoint.x, currentPoint.y, N, N)) - referenceFrame(cv::Rect(referencePoint.x, referencePoint.y, N, N))))[0];
						}
						else if (similarityMeasure == 1) {
							//Calculate SSD
							similarity = cv::sum(currentFrame(cv::Rect(currentPoint.x, currentPoint.y, N, N)) - referenceFrame(cv::Rect(referencePoint.x, referencePoint.y, N, N)))[0];
							similarity *= similarity;
						}

						//Take the closest lowest error
						float distance = sqrt((float)(((referencePoint.x - currentPoint.x) * (referencePoint.x - currentPoint.x)) + ((referencePoint.y - currentPoint.y) * (referencePoint.y - currentPoint.y))));

						//Write buffer with the lowest error
						if (similarity < lowestSimilarity || (similarity == lowestSimilarity && distance <= blockDistance)) {
							lowestSimilarity = similarity;
							blockDistance = distance;
							float p0x = currentPoint.x, p0y = currentPoint.y - sqrt((float)(((referencePoint.x - p0x) * (referencePoint.x - p0x)) + ((referencePoint.y - currentPoint.y) * (referencePoint.y - currentPoint.y))));
							float angle = (2 * atan2(referencePoint.y - p0y, referencePoint.x - p0x)) * 180 / M_PI;
							motion[idx] = referencePoint;
							details[idx] = cv::Point2f(angle, blockDistance);
						}
					}
				}
			}
		}
	}
}