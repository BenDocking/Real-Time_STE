//#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
//#define __CL_ENABLE_EXCEPTIONS
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <opencv2/opencv.hpp>

#ifdef __APPLE__
#include <OpenCL/cl.hpp>
#else
//#include <CL/cl.hpp>
#endif

//#include "Utils.h"

using namespace std;
using namespace cv;
//using namespace cl;


struct Frame {
	int Xindices;
	int Yindices;
	double Sum_MisMatch[1][1][2401];
	int Flag;
	int Flag_Regularised;
	double Skew;
	double Magnitude;
	double Contrast;
	double Curvature;
	double Quality_Score;
	double Neighbour_Angle;
	double Neighbour_Magnitude;
	double NoNei;
	double Correlation;
	double Overal_DispY;
} Frames[505];

struct SpeckleTracking {
	int ColImage;
	int RowImage;
	int KernelCol;
	int KernelRow;
	int KernelStepSize;
	int ColSW;
	int RowSW;
	int SWStepSize;
	int HfKernelCol;
	int HfKernelRow;
	int NoPix;
	int TotalBlocks;
	int RefKernel[121];
	int RefSearchBlock[2401];
	int indexX[2401];
	int indexY[2401];
	int SearchBlocksRows;
	int SearchBlocksCols;
	int KernelRow_Global[600][800];
	int KernelCol_Global[600][800];
	int KernelY;
	double CornerKernelY;
	int CornerSWY;
	int KernelX;
	double CornerKernelX;
	int CornerSWX;
} ST;

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
	int type; //.b8
	/// number of frames in file
	int frames; //length of file...frames
	/// width - number of vectors for raw data, image width for processed data
	int w; //width
	/// height - number of samples for raw data, image height for processed data
	int h; //height
	/// data sample size in bits
	int ss; //size of data in bits
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
	int indexp1;
	int hdr_frames = hdr.frames;
	int FrameRate = hdr.dr;
	int size_Sonix = 0;
	IplImage * ImageRaw;
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
			
			// display image
			ImageRaw = cvCreateImage(cvSize(hdr.w, hdr.h), IPL_DEPTH_8U, 3);
			for (int pixelp = 0; pixelp < hdr.w * hdr.h; pixelp++) {
				indexp1 = pixelp * 3;
			ImageRaw->imageData[indexp1] = ImageRaw->imageData[indexp1+1] = ImageRaw->imageData[indexp1+2] = sr[fr].Image[pixelp];
			}

			cvFlip(ImageRaw, NULL, 1);
			cvShowImage("Raw_Image",ImageRaw);
			cv::waitKey(1); // wait for 1000 milliseconds
			cvReleaseImage(&ImageRaw);
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

void BlockMatching() {

}

int main(int argc, char **argv) {
	FILE *fp = ReadFile();
	uFileHeader hdr = ReadHeader(fp);
	Speckle_Results *sr = ReadImageData(fp, hdr);
	system("pause");
	BlockMatching();
	exit(-1);
	return 0;
}