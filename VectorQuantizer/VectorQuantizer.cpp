#include"VectorQuantizer.h"
#include<iostream>
#include<cmath>
using std::ios;
using std::noskipws;
using std::cout;
using std::cerr;
using std::endl;
using std::abs;
using std::log10;
using std::log2;
using std::pow;
using std::sqrt;

VectorQuantizer::VectorQuantizer(int ib, int bs, int is, int cs)
{
	intensityBit = ib;
	blockSize = bs;
	imageSize = is;
	codebookSize = cs;
	/* initialize the image matrix by all 0's */
	for (int i = 0; i < imageSize; ++i) {
		vector<unsigned char> tempRow;
		for (int j = 0; j < imageSize; ++j) {
			tempRow.push_back('\0');
		}
		imageMatrix.push_back(tempRow);
	}

	/* initialize code book */
	for (int i = 0; i < codebookSize; ++i) {
		vector<float> codeword;
		for (int j = 0; j < blockSize * blockSize; ++j) {
			codeword.push_back(0);
		}
		codebook.push_back(codeword);
	}

	/* initialize temp block vectors */
	long numOfBlocks = (imageSize / blockSize) * (imageSize / blockSize);
	for (long i = 0; i < numOfBlocks; ++i) {
		vector<float> tempBlock;
		for (int j = 0; j < blockSize * blockSize; ++j) {
			tempBlock.push_back(0);
		}
		imageBlocks.push_back(tempBlock);
	}
}

void VectorQuantizer::loadImage(string fileName)
{
	inFile.open(fileName, ios::binary);
	unsigned char readNext;
	int countRow = 0, countCol = 0;
	while (inFile >> noskipws >> readNext) {
		++countCol;
		imageMatrix[countRow][countCol - 1] = readNext;
		if (countCol == imageSize) {
			++countRow;
			countCol = 0;
		}
	}

	inFile.close();
}

void VectorQuantizer::printImageMatrix()
{
	for (int i = 0; i < imageSize; ++i) {
		for (int j = 0; j < imageSize; ++j) {
			cout << imageMatrix[i][j];
		}
		cout << endl;
	}
}

void VectorQuantizer::imageBlocking(Aim TorQ)
{
	/* the arrangement of blocks over the whole image is from left to right,
	from top to bottom, so is the arrangement of pixels inside each block
	when putting them into vectors */

	/* save pixel values in block vectors, must be general man! */
	for (int i = 0; i < imageSize; ++i) {
		for (int j = 0; j < imageSize; ++j) {
			imageBlocks[(i / blockSize) * (imageSize / blockSize) + j / blockSize]
				[(i % blockSize) * blockSize + j % blockSize] = float(imageMatrix[i][j]);
		}
	}

	switch (TorQ) {
	case Train:
		/* since we'll use more than one image for training, every time receiving a new image,
		block it into the tempBlockVectors first, then append to blockVectors*/
		for (long i = 0; i < imageBlocks.size(); ++i) {
			trainingBlocks.push_back(new BlockVector(imageBlocks[i], 0));
		}
		// cout << blockVectors.size() << endl;
		break;

	case Quantize:
		quantizeBlocks.clear();
		for (long i = 0; i < imageBlocks.size(); ++i) {
			quantizeBlocks.push_back(new BlockVector(imageBlocks[i], 0));
		}
		break;

	default:
		cerr << "Enter a purpose!" << endl;
		break;
	}
	
}

float VectorQuantizer::vectorDist(const vector<float> &v_1, const vector<float> &v_2)
{
	float dist = 0;
	for (int i = 0; i < v_1.size(); ++i)
		dist += (v_1[i] - v_2[i]) * (v_1[i] - v_2[i]);
	return (sqrt(dist));
	
}

void VectorQuantizer::kmeanInitial()
{
	// distance of each vector
	for (int i = 0; i < codebookSize; ++i) {
		float max = 0;
		// the first centroid (codeword) is the vector with max 2-norm
		if (i == 0) {
			for (long j = 0; j < trainingBlocks.size(); ++j) {
				float dist =
					vectorDist(trainingBlocks[j]->block, codebook[i + 1]);
				if (dist > max) {
					max = dist;
					codebook[i] = trainingBlocks[j]->block;
				}
				dist2cent.push_back(dist);
			}
			// cout << max << endl;
		}

		else {
			for (long j = 0; j < trainingBlocks.size(); ++j) {
				float dist =
					vectorDist(trainingBlocks[j]->block, codebook[i - 1]);
				// use the smallest distance to a centroid as the "distance"
				// of the vector
				if (dist < dist2cent[j]) {
					dist2cent[j] = dist;
					trainingBlocks[j]->index_k = i;
				}
				if (dist2cent[j] > max) {
					max = dist2cent[j];
					codebook[i] = trainingBlocks[j]->block;
				}
			}
			// cout << max << endl;
		}
		/*for (int j = 0; j < blockSize * blockSize; ++j)
			cout << codebook[i][j] << " ";
		cout << endl;*/
	}
}

void VectorQuantizer::kmean(float eps)
{
	/* initialization */
	kmeanInitial();

	float MSE_1 = 0, MSE_2 = 0;
	// initial MSE_2
	for (long i = 0; i < trainingBlocks.size(); ++i) {
		MSE_2 += pow(dist2cent[i], 2) / trainingBlocks.size();
	}

	int countIteration = 0;
	while (abs(MSE_1 - MSE_2) / MSE_2 >= eps) {
		++countIteration;
		cout << "Iteration " << countIteration << '\t'
			<< "MSE = " << MSE_2 << endl;

		// compute new mean of each cluster
		for (int i = 0; i < codebookSize; ++i) {
			codebook[i] = clusterMean(i);
		}

		// re-cluster
		for (long i = 0; i < trainingBlocks.size(); ++i) {
			float dist;
			for (int j = 0; j < codebookSize; ++j) {
				dist =
					vectorDist(trainingBlocks[i]->block, codebook[j]);
				if (dist < dist2cent[i]) {
					dist2cent[i] = dist;
					trainingBlocks[i]->index_k = j;
				}
			}
		}

		// new MSE
		MSE_1 = MSE_2;
		MSE_2 = 0;
		for (long i = 0; i < trainingBlocks.size(); ++i) {
			MSE_2 += pow(dist2cent[i], 2) / trainingBlocks.size();
		}
	}
}

vector<float> VectorQuantizer::clusterMean(int idx)
{
	vector<long> clusterSum;
	for (int i = 0; i < blockSize * blockSize; ++i) {
		clusterSum.push_back(0);
	}

	/* sum all vectors in one cluster */
	long countInCluster = 0;
	for (vector<BV>::iterator i = trainingBlocks.begin(); i != trainingBlocks.end(); ++i) {
		if ((*i)->index_k == idx) {
			++countInCluster;
			for (int j = 0; j < blockSize * blockSize; ++j) {
				clusterSum[j] += (*i)->block[j];
			}
		}
	}

	/* compute mean of each dimension */
	vector<float> mean;
	for (int i = 0; i < clusterSum.size(); ++i) {
		mean.push_back(float(clusterSum[i]) / countInCluster);
	}
	
	return mean;
}

void VectorQuantizer::quantize(string fileName)
{
	/* load and block the new image to be quantized */
	loadImage(fileName);
	imageBlocking(Quantize);

	/* find the closest centroid */
	vector<float> quantizeDist, codewordFreq;
	for (int i = 0; i < codebookSize; ++i) {
		codewordFreq.push_back(0);
	}

	float MSE = 0, PSNR;
	for (long i = 0; i < quantizeBlocks.size(); ++i) {
		float dist;
		for (int j = 0; j < codebookSize; ++j) {
			if (j == 0) {
				dist =
					vectorDist(quantizeBlocks[i]->block, codebook[j]);
				quantizeDist.push_back(dist);
				quantizeBlocks[i]->index_k = j;
			}
			else {
				dist =
					vectorDist(quantizeBlocks[i]->block, codebook[j]);
				if (dist < quantizeDist[i]) {
					quantizeDist[i] = dist;
					quantizeBlocks[i]->index_k = j;
				}
			}	
		}
		// after compare with all code words, pick the nearest one as the quantization level
		quantizeBlocks[i]->block = codebook[quantizeBlocks[i]->index_k];
		// add the error
		MSE += pow(quantizeDist[i], 2) / quantizeBlocks.size();
	}

	/* codeword frequency */
	for (vector<BV>::iterator i = quantizeBlocks.begin(); i != quantizeBlocks.end(); ++i) {
		++codewordFreq[(*i)->index_k];
	}
	for (int i = 0; i < codebookSize; ++i) {
		codewordFreq[i] /= quantizeBlocks.size();
	}

	/* PSNR */
	PSNR = 10 * log10(pow(pow(2, intensityBit) - 1, 2) / MSE);

	/* entropy */
	float entropy = 0;
	for (int i = 0; i < codebookSize; ++i) {
		if (codewordFreq[i] == 0)
			continue;
		entropy -= codewordFreq[i] * log2(codewordFreq[i]);
	}

	cout << "MSE = " << MSE << '\t' << "PSNR = " << PSNR
		<< '\t' << "Entropy = " << entropy << "bits/pixel" << endl;
}