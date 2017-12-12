//Create sampler for image2d_t that doesnt interpolate points, and sets out of bound pixels to 0
__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

int SumAbsoluteDifference(image2d_t currentImage, image2d_t referenceImage, int2 currentPoint, int2 referencePoint, int N) {
	int sum = 0;
	int offset = N / 2;

	for (int i = 0; i < N; i++)
	{
		for (int j = 0; j < N; j++)
		{
			int current = read_imageui(currentImage, sampler, (int2)(currentPoint.x + i - offset, currentPoint.y + j - offset)).x;
			int reference = read_imageui(referenceImage, sampler, (int2)(referencePoint.x + i - offset, referencePoint.y + j - offset)).x;
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