//Create sampler for image2d_t that doesnt interpolate points, and sets out of bound pixels to 0
__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

int SumAbsoluteDifference(image2d_t currentImage, image2d_t referenceImage, int2 currentPoint, int2 referencePoint, int N) {
	int sum = 0;

	for (int i = 0; i < N; i++)
	{
		for (int j = 0; j < N; j++)
		{
			int current = read_imageui(currentImage, sampler, (int2)(currentPoint.x + i, currentPoint.y + j)).x;
			int reference = read_imageui(referenceImage, sampler, (int2)(referencePoint.x + i, referencePoint.y + j)).x;
			sum += current < reference ? reference - current : current - reference;
		}
	}

	return sum;
}

__kernel void ExhaustiveBlockMatchingSAD(
	__read_only image2d_t currentFrame,
	__read_only image2d_t referenceFrame,
	const int N, //blockSize
	const uint stepSize,
	const int width,
	const int height,
	__global int2 * motion,
	__global float2 * details)
{
	const int x = get_global_id(0); //w_g width ID
	const int y = get_global_id(1); //w_g height ID
	const int blocksW = get_global_size(0); //w_g width size
	const int idx = x + y * blocksW; //1D idientifier 

	const int2 currentPoint = { x * stepSize, y * stepSize };
	
	float dist = FLT_MAX;
	float lowestSimilarity = FLT_MAX;
	float similarityMeasure;

	//search window = 20 * 20
	for (int i = -N; i < N; i++) { 
		for (int j = -N; j < N; j++) { 
			//referencePoint = current point of reference in search window
			int2 referencePoint = { currentPoint.x + i, currentPoint.y + j };
			//is block within bounds
			if (referencePoint.y >= 0 && referencePoint.y < (height - N) && referencePoint.x >= 0 && referencePoint.x < (width - N)) { 
				//calculate sum absolute difference
				similarityMeasure = SumAbsoluteDifference(currentFrame, referenceFrame, currentPoint, referencePoint, N);

				//prefer nearer blocks
				float currentDist = sqrt((float)(((referencePoint.x - currentPoint.x) * (referencePoint.x - currentPoint.x)) + ((referencePoint.y - currentPoint.y) * (referencePoint.y - currentPoint.y))));

				if (similarityMeasure < lowestSimilarity || (similarityMeasure == lowestSimilarity && currentDist <= dist)) { 
					lowestSimilarity = similarityMeasure;
					dist = currentDist;
					float p0x = currentPoint.x;
					float p0y = currentPoint.y - sqrt((float)(((referencePoint.x - p0x) * (referencePoint.x - p0x)) + ((referencePoint.y - currentPoint.y) * (referencePoint.y - currentPoint.y))));
					float angle = (2 * atan2(referencePoint.y - p0y, referencePoint.x - p0x)) * 180 / M_PI;
					motion[idx] = referencePoint;
					details[idx] = (float2)(angle, dist); 
				}
			}
		}
	}
}

//a simple OpenCL kernel which copies all pixels from A to B
__kernel void identity(__global const uchar4* A, __global uchar4* B) {
	int id = get_global_id(0);
	B[id] = A[id];
}

//simple 2D identity kernel
__kernel void identity2D(__global const uchar4* A, __global uchar4* B) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0); //width in pixels
	int id = x + y*width;
	B[id] = A[id];
}

//2D averaging filter
__kernel void avg_filter2D(__global const uchar4* A, __global uchar4* B) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0); //width in pixels
	int id = x + y*width;

	uint4 result = (uint4)(0);//zero all 4 components

	for (int i = (x-1); i <= (x+1); i++)
	for (int j = (y-1); j <= (y+1); j++) 
		result += convert_uint4(A[i + j*width]); //convert pixel values to uint4 so the sum can be larger than 255

	result /= (uint4)(9); //normalise all components (the result is a sum of 9 values) 

	B[id] = convert_uchar4(result); //convert back to uchar4 
}

//2D 3x3 convolution kernel
__kernel void convolution2D(__global const uchar4* A, __global uchar4* B, __constant float* mask) {
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0); //width in pixels
	int id = x + y*width;

	float4 result = (float4)(0.0f,0.0f,0.0f,0.0f);//zero all 4 components

	for (int i = (x-1); i <= (x+1); i++)
	for (int j = (y-1); j <= (y+1); j++)
		result += convert_float4(A[i + j*width])*(float4)(mask[i-(x-1) + (j-(y-1))*3]);//convert pixel and mask values to float4

	B[id] = convert_uchar4(result); //convert back to uchar4
}
