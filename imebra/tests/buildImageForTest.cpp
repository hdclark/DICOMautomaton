#include "buildImageForTest.h"
#include <stdlib.h>

#include <QtTest/QtTest>

namespace puntoexe
{

namespace imebra
{

namespace tests
{


puntoexe::ptr<puntoexe::imebra::image> buildImageForTest(
	imbxUint32 pixelsX, 
	imbxUint32 pixelsY, 
	puntoexe::imebra::image::bitDepth depth,
	imbxUint32 highBit, 
	double sizeX, 
	double sizeY, 
	std::wstring colorSpace, 
	imbxUint32 continuity)
{
	puntoexe::ptr<puntoexe::imebra::image> newImage(new puntoexe::imebra::image);
	puntoexe::ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> imgHandler = newImage->create(pixelsX, pixelsY, depth, colorSpace, highBit);
	imbxUint32 channelsNumber = newImage->getChannelsNumber();

	imbxInt32 range = (imbxUint32)1 << highBit;
	imbxInt32 minValue = 0;
	if(depth == puntoexe::imebra::image::depthS16 || depth == puntoexe::imebra::image::depthS8)
	{
		minValue = -1 << (highBit - 1);
	}
	imbxInt32 maxValue(minValue + range);

	imbxUint32 index(0);
	for(imbxUint32 scanY(0); scanY != pixelsY; ++scanY)
	{
		for(imbxUint32 scanX(0); scanX != pixelsX; ++scanX)
		{
			for(imbxUint32 scanChannels = 0; scanChannels != channelsNumber; ++scanChannels)
			{
				imbxInt32 value = (imbxInt32)(((double)((scanX * channelsNumber+ scanY + scanChannels) % continuity) / (double)continuity) * (double)range)  + minValue;
 				if(value < minValue)
				{
					value = minValue;
				}
				if(value >= maxValue)
				{
					value = maxValue - 1;
				}
				imgHandler->setSignedLong(index++, value);
			}
		}
	}

	newImage->setSizeMm(sizeX, sizeY);

	return newImage;
}


double compareImages(ptr<image> image0, ptr<image> image1)
{
	imbxUint32 sizeX0, sizeY0, sizeX1, sizeY1;
	image0->getSize(&sizeX0, &sizeY0);
	image1->getSize(&sizeX1, &sizeY1);
	if(sizeX0 != sizeX1 || sizeY0 != sizeY1)
	{
		return 1000;
	}

	imbxUint32 rowSize, channelSize, channelsNumber0, channelsNumber1;
	ptr<handlers::dataHandlerNumericBase> hImage0 = image0->getDataHandler(false, &rowSize, &channelSize, &channelsNumber0);
	ptr<handlers::dataHandlerNumericBase> hImage1 = image1->getDataHandler(false, &rowSize, &channelSize, &channelsNumber1);
	if(channelsNumber0 != channelsNumber1)
	{
		return 1000;
	}

	imbxUint32 highBit0 = image0->getHighBit();
	imbxUint32 highBit1 = image1->getHighBit();
	if(highBit0 != highBit1)
	{
		return 1000;
	}

	image::bitDepth depth0 = image0->getDepth();
	image::bitDepth depth1 = image1->getDepth();
	if(depth0 != depth1)
	{
		return 1000;
	}
	
	if(sizeX0 == 0 || sizeY0 == 0)
	{
		return 0;
	}

	imbxUint32 valuesNum = sizeX0 * sizeY0 * channelsNumber0;
	double divisor = double(valuesNum);
	double range = (double)(1 << image0->getHighBit());
	double difference(0);
	int index(0);
	unsigned long long total(0);
	while(valuesNum--)
	{
		difference += 1000 * (double)labs(hImage0->getSignedLong(index) - hImage1->getSignedLong(index)) / range;
		total += labs(hImage0->getSignedLong(index));
		++index;
	}

	difference /= divisor;

	return difference;
}


} // namespace tests

} // namespace imebra

} // namespace puntoexe
