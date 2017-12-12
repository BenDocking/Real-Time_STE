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
#include <time.h>

#include "Utils.h"

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
void BlockMatchingSerial(uFileHeader hdr, Speckle_Results *sr);
cv::Vec3i Closest(const cv::Mat& referenceFrame, const cv::Point& currentPoint, const int searchWindow, const int width, const int height, const int N);
void BlockMatchingFrame(cv::Mat& currentFrame, cv::Mat& referenceFrame, cv::Point * &motion, cv::Point2f * &details, int N, int stepSize, int width, int height, int blocksW, int blocksH, int similarityMeasure);
void BlockMatchingParallel(uFileHeader hdr, Speckle_Results *sr, cl::Context context, cl::Program program, cl::CommandQueue queue);

void print_help() {
	std::cerr << "Application usage:" << std::endl;

	std::cerr << "  -p : select platform " << std::endl;
	std::cerr << "  -d : select device" << std::endl;
	std::cerr << "  -l : list all platforms and devices" << std::endl;
	std::cerr << "  -h : print this message" << std::endl;
}

int main(int argc, char **argv) {
	//handle command line options such as device selection, verbosity, etc.
	int platform_id = 0;
	int device_id = 0;

	for (int i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-p") == 0) && (i < (argc - 1))) { platform_id = atoi(argv[++i]); }
		else if ((strcmp(argv[i], "-d") == 0) && (i < (argc - 1))) { device_id = atoi(argv[++i]); }
		else if (strcmp(argv[i], "-l") == 0) { std::cout << ListPlatformsDevices() << std::endl; }
		else if (strcmp(argv[i], "-h") == 0) { print_help(); }
	}

	//detect any potential exceptions
	try {
		//host operations
		//Select computing devices
		cl::Context context = GetContext(platform_id, device_id);

		//display the selected device
		std::cout << "Running on " << GetPlatformName(platform_id) << ", " << GetDeviceName(platform_id, device_id) << std::endl;

		//Load & build the device code
		cl::Program::Sources sources;

		AddSources(sources, "C:\\Users\\user\\Documents\\GitHub\\Real-Time_STE\\Eulerian_ST\\Eulerian_ST\\kernels.cl");

		cl::Program program(context, sources);

		//build and debug the kernel code
		try {
			program.build();
		}
		catch (const cl::Error& err) {
			std::cout << "Build Status: " << program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(context.getInfo<CL_CONTEXT_DEVICES>()[0]) << std::endl;
			std::cout << "Build Options:\t" << program.getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(context.getInfo<CL_CONTEXT_DEVICES>()[0]) << std::endl;
			std::cout << "Build Log:\t " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(context.getInfo<CL_CONTEXT_DEVICES>()[0]) << std::endl;
			char * i = new char[200];
			std::cin >> i;
			throw err;
		}

		//create a queue to which we will push commands for the device
		cl::CommandQueue queue(context);

		//read and store data
		FILE *fp = ReadFile();
		clock_t timer = clock();
		uFileHeader hdr = ReadHeader(fp);
		Speckle_Results *sr = ReadImageData(fp, hdr);
		timer = clock() - timer;
		printf("It took %f seconds to read data from file pointer.\n\n", ((float)timer) / CLOCKS_PER_SEC);
		printf("Press 'q' to quit.");
		//BlockMatchingSerial(hdr, sr);
		BlockMatchingParallel(hdr, sr, context, program, queue);
		cv::destroyAllWindows();
		system("pause");
		exit(-1);
		return 0;
	}
	catch (const cl::Error& err) {
		std::cerr << "ERROR: " << err.what() << ", " << getErrorString(err.err()) << std::endl;
	}
	catch (const exception& exc) {
		std::cerr << "ERROR: " << exc.what() << std::endl;
	}

	system("pause");
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

void BlockMatchingSerial(uFileHeader hdr, Speckle_Results *sr) {
	int N = 10; //block size
	double framerate;
	int stepSize = 5; //step size of blocks
	int blocksH = ceil(hdr.h / stepSize);
	int blocksW = ceil(hdr.w / stepSize);
	int similarityMeasure; //0 = SAD .. 1 = SSD 
	time_t timer;
	cv::Mat currentFrame;
	cv::Mat referenceFrame;
	//point array for outputs
	cv::Point * motion = new cv::Point[blocksH * blocksW];
	cv::Point2f * details = new cv::Point2f[blocksH * blocksW];

	std::cout << "Enter number corresponding to similarity measure:\n\n1. SAD\n2. SSD\n\n";
	std::cin >> similarityMeasure;
	similarityMeasure--;

	std::string window = "BM";
	cv::namedWindow(window, cv::WINDOW_AUTOSIZE);

	//cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
	//checkErr(err, "Cannot create the command queue");

	//loop through frames
	for (int fr = 1; fr < hdr.frames; fr++) {
		timer = clock();
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
				//for (std::size_t x = 0; x < (N * N); x++) {
				//	intensity = intensity + currentFrame.at<uchar>(j * stepSize, i * stepSize);
				//}
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
		timer = clock() - timer;
		framerate = 1 / (((float)timer) / CLOCKS_PER_SEC);
		char frameInfo[200];
		sprintf(frameInfo, "Frame %d of %d", fr, hdr.frames);
		cv::putText(display, frameInfo, cvPoint(30, 30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(200, 200, 250), 1, CV_AA);
		sprintf(frameInfo, "Frame rate: %f", framerate);
		cv::putText(display, frameInfo, cvPoint(30, 45), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(200, 200, 250), 1, CV_AA);
		sprintf(frameInfo, "Frame time: %f", ((float)timer) / CLOCKS_PER_SEC);
		cv::putText(display, frameInfo, cvPoint(30, 60), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(200, 200, 250), 1, CV_AA);
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

void BlockMatchingParallel(uFileHeader hdr, Speckle_Results *sr, cl::Context context, cl::Program program, cl::CommandQueue queue) {
	char x;
	bool loop = true;
	int N = 21; //block size
	double framerate;
	int stepSize = 10; //step size of blocks
	int blocksH = ceil(hdr.h / stepSize);
	int blocksW = ceil(hdr.w / stepSize);
	int similarityMeasure; //0 = SAD .. 1 = SSD 
	time_t timer;
	cv::Mat currentFrame;
	cv::Mat referenceFrame;
	std::string window1 = "Parallel_Block_Matching_Image_1";
	cv::namedWindow(window1, cv::WINDOW_AUTOSIZE);
	std::string window2 = "Parallel_Block_Matching_Image_2";
	cv::namedWindow(window2, cv::WINDOW_AUTOSIZE);

	while (loop == true) {
		for (int fr = 1; fr < hdr.frames; fr++) {
			timer = clock();
			//forwards prediction
			referenceFrame = convertMat(hdr.w, hdr.h, fr - 1, sr);
			currentFrame = convertMat(hdr.w, hdr.h, fr, sr);

			char * currentBuffer = reinterpret_cast<char *>(currentFrame.data);
			char * referenceBuffer = reinterpret_cast<char *>(referenceFrame.data);


			cl::ImageFormat format(CL_R, CL_UNSIGNED_INT8);
			cl::Image2D referenceImage(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, format, hdr.w, hdr.h, 0, referenceBuffer);
			cl::Image2D currentImage(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, format, hdr.w, hdr.h, 0, currentBuffer);

			//create image detail buffers
			cl::Buffer current(context, CL_MEM_READ_ONLY, sizeof(char) * (hdr.w * hdr.h));
			cl::Buffer reference(context, CL_MEM_READ_ONLY, sizeof(char) * (hdr.w * hdr.h));

			//Create motion buffers to store motion for blocks
			cl::Buffer motion(context, CL_MEM_WRITE_ONLY, sizeof(cl_int2) * (blocksH * blocksW));
			cl::Buffer details(context, CL_MEM_WRITE_ONLY, sizeof(cl_float2) * (blocksH * blocksW));

			queue.enqueueWriteBuffer(current, CL_TRUE, 0, sizeof(char) * (hdr.w * hdr.h), &currentBuffer[0]);
			queue.enqueueWriteBuffer(reference, CL_TRUE, 0, sizeof(char) * (hdr.w * hdr.h), &referenceBuffer[0]);


			//set arguments and create kernel
			cl::Kernel kernel(program, "ExhaustiveBlockMatchingSAD");
			kernel.setArg(0, currentImage);
			kernel.setArg(1, referenceImage);
			kernel.setArg(2, N);
			kernel.setArg(3, stepSize);
			kernel.setArg(4, hdr.w);
			kernel.setArg(5, hdr.h);
			kernel.setArg(6, motion);
			kernel.setArg(7, details);

			//Queue kernel with 1D global range spanning all blocks
			cl::NDRange global((size_t)blocksW, (size_t)blocksH, 1);
			queue.enqueueNDRangeKernel(kernel, 0, global, cl::NullRange);

			cl_int2 * motionBuffer = new cl_int2[blocksH * blocksW];
			cl_float2 * detailsBuffer = new cl_float2[blocksH * blocksW];

			//Read motion and details buffer from device
			queue.enqueueReadBuffer(motion, CL_TRUE, 0, sizeof(cl_int2) * (blocksH * blocksW), &motionBuffer[0]);
			queue.enqueueReadBuffer(details, CL_TRUE, 0, sizeof(cl_float2) * (blocksH * blocksW), &detailsBuffer[0]);

			timer = clock() - timer;

			cv::Mat display;
			cvtColor(currentFrame, display, CV_GRAY2RGB);
			cv::Mat displayBlack(hdr.h, hdr.w, CV_8UC3, cv::Scalar(0, 0, 0));

			bool drawGrid = false;
			cv::Scalar rectColour = cv::Scalar(255);
			cv::Scalar lineColour = cv::Scalar(0, 0, 255);

			for (std::size_t i = 0; i < blocksW - 1; i++)
			{
				for (std::size_t j = 0; j < blocksH - 1; j++)
				{
					cv::Scalar intensity = currentFrame.at<uchar>(j * stepSize, i * stepSize);

					int iVal = intensity.val[0];
					//if (iVal < 90) {
					//	lineColour = cv::Scalar(0, 255, 255);
					//}
					//else {
					//	lineColour = cv::Scalar(0, 0, 255);
					//}
					if (iVal > -1) {
						//Calculate repective position of motion vector
						int idx = i + j * blocksW;

						//Offset drawn point to represent middle rather than top left of block
						cv::Point offset(N / 2, N / 2);
						cv::Point pos(i * stepSize, j * stepSize);
						cv::Point mVec(motionBuffer[idx].x, motionBuffer[idx].y);

						if (drawGrid) {
							rectangle(display, pos, pos + cv::Point(N, N), rectColour);
						}
						//only display vectors with motion
						//if (detailsBuffer[idx].y != 0 || detailsBuffer[idx].y != 0) {
						arrowedLine(display, pos + offset, mVec + offset, lineColour);
						arrowedLine(displayBlack, pos + offset, mVec + offset, lineColour);
						//if (i == 60 && j == 60)
							//std::cout << pos << ":" << mVec << std::endl;
					//}
					}
				}
			}

			framerate = 1 / (((float)timer) / CLOCKS_PER_SEC);
			char frameInfo[200];
			sprintf(frameInfo, "Frame %d of %d", fr, hdr.frames);
			cv::putText(display, frameInfo, cvPoint(30, 30), cv::FONT_HERSHEY_COMPLEX, 0.4, cvScalar(200, 200, 250), 0.7, CV_AA);
			sprintf(frameInfo, "Frame rate: %f", framerate);
			cv::putText(display, frameInfo, cvPoint(30, 45), cv::FONT_HERSHEY_COMPLEX, 0.4, cvScalar(200, 200, 250), 0.7, CV_AA);
			sprintf(frameInfo, "Frame time: %f", ((float)timer) / CLOCKS_PER_SEC);
			cv::putText(display, frameInfo, cvPoint(30, 60), cv::FONT_HERSHEY_COMPLEX, 0.4, cvScalar(200, 200, 250), 0.7, CV_AA);
			sprintf(frameInfo, "Search window size: %d X %d", (N * 2 - 1), (N * 2 - 1));
			cv::putText(display, frameInfo, cvPoint(30, 75), cv::FONT_HERSHEY_COMPLEX, 0.4, cvScalar(200, 200, 250), 0.7, CV_AA);
			sprintf(frameInfo, "Block size: %d x %d", N, N);
			cv::putText(display, frameInfo, cvPoint(30, 90), cv::FONT_HERSHEY_COMPLEX, 0.4, cvScalar(200, 200, 250), 0.7, CV_AA);
			sprintf(frameInfo, "Step size: %d", stepSize);
			cv::putText(display, frameInfo, cvPoint(30, 105), cv::FONT_HERSHEY_COMPLEX, 0.4, cvScalar(200, 200, 250), 0.7, CV_AA);
			imshow(window1, display);
			imshow(window2, displayBlack);
			char x = cv::waitKey(1);
			if (x == 'q')
				exit(-1);
		}
	}
}