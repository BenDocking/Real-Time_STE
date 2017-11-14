//#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
//#define __CL_ENABLE_EXCEPTIONS
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <vector>
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
void DisplayImage(int fr, int width, int height, Speckle_Results *sr);
FILE* ReadFile();
uFileHeader ReadHeader(FILE *fp);
Speckle_Results* ReadImageData(FILE *fp, uFileHeader hdr);
Mat convertMat(int hdr_width, int hdr_height, int fr, Speckle_Results *sr);
void BlockMatching(uFileHeader hdr, Speckle_Results *sr);

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

void DisplayImage(int fr, int width, int height, Speckle_Results *sr) {
	Mat ImageRaw = Mat(cvSize(width, height), CV_8UC1);
	for (int pixelp = 0; pixelp < width * height; pixelp++) {
		ImageRaw.data[pixelp] = sr[fr].Image[pixelp];
	}
	namedWindow("Raw_Image", WINDOW_AUTOSIZE);
	imshow("Raw_Image", ImageRaw);
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
			DisplayImage(fr, hdr.w, hdr.h, sr); //display image frame
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
	double * MAD_Matrix = new double[2 * M + 1];
	double * mvx = new double[2 * M + 1];
	double * mvy = new double[2 * M + 1];
	int hdr_height = hdr.h;
	int hdr_width = hdr.w;
	double MAD; //mean of differences between reference and target image
	double A; //reference image
	double B; //target image
	double MAD_Min  = 9999999999;
	Mat TargetFrame;
	Mat ReferenceFrame;
	int ctr = 0;
	int index, iblk, jblk, dy, dx;
	//output variables
	double * MAD_Min_Blocks = new double[((hdr_height + N - 1) / N) * ((hdr_width + N - 1) / N)];
	int * mvx_Blocks = new int[((hdr_height + N - 1) / N) * ((hdr_width + N - 1) / N)];
	int * mvy_Blocks = new int[((hdr_height + N - 1) / N) * ((hdr_width + N - 1) / N)];

	//loop through frames
	for (int fr = 0; fr < hdr.frames; fr++) {
		ReferenceFrame = convertMat(hdr_width, hdr_height, fr, sr);
		TargetFrame = convertMat(hdr_width, hdr_height, fr+1, sr);
		//loop through image
		for (int i = 0; i < hdr_height - N - 1; i = i + N) {
			for (int j = 0; j < hdr_width - N - 1; j = j + N) {
				ctr = 0;
				//loop through search window
				for (int x = -M; x <= M; x++) {
					for (int y = -M; y <= M; y++) {
						MAD = 0; //reset MAD
						//u and v = displacement for height and width
						for (int u = 0; u < N; u++) {
							for (int v = 0; v < N; v++) {
								if ((u + x > 0) && (u + x < hdr_height + 1) && (v + y > 0) && (v + y < hdr_width + 1)) {
									//backward prediction
									A = (double)ReferenceFrame.at<uchar>((i + u), (j + v));
									B = (double)TargetFrame.at<uchar>((i + u + x), (j + v + y));
									MAD += fabs(A - B);
								}
							}
						}
						MAD /= N * N;
						MAD_Matrix[ctr] = MAD;
						mvx[ctr] = x;
						mvy[ctr] = y;
						ctr++;
					}
				}
				//get min value of MAD_Matrix
				for (int m = 0; m < ctr - 1; m++) {
					double curr = MAD_Matrix[m];
					if (curr < MAD_Min) {
						MAD_Min = curr;
						index = m; //store index of minumim MAD
					}
				}
				dx = index % ((hdr_width + N - 1) / N);
				dy = index / ((hdr_width + N - 1) / N);
				dx -= 10; //between -10 and 10
				dx -= 10;
				iblk = (i + N - 1) / N;
				jblk = (j + N - 1) / N;
				//outputs
				mvx_Blocks[iblk * ((hdr_width + N - 1) / N) + jblk] = dx;
				mvy_Blocks[iblk * ((hdr_width + N - 1) / N) + jblk] = dy;
				MAD_Min_Blocks[iblk * ((hdr_width + N - 1) / N) + jblk] = MAD_Min;
			}
		}
	}
	delete[] mvx_Blocks;
	delete[] mvy_Blocks;
	delete[] MAD_Min_Blocks;
	delete[] MAD_Matrix;
	delete[] mvx;
	delete[] mvy;
}