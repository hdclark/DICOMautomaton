#include "drawBitmapTest.h"

#include "../library/imebra/include/imebra.h"
#include "buildImageForTest.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

using namespace puntoexe::imebra;

void drawBitmapTest::testDrawBitmap()
{
	for(int monochrome(0); monochrome != 2; ++monochrome)
	{
		for(int subsample = 1; subsample != 4; subsample <<= 1)
		{
			ptr<image> testImage(buildImageForTest(
					400,
					300,
					puntoexe::imebra::image::depthU8,
					7,
					400,
					300,
					monochrome == 1 ? L"MONOCHROME2" : L"RGB",
					50));

			ptr<dataSet> testDataSet(new dataSet);
			testDataSet->setImage(0, testImage, L"1.2.840.10008.1.2", codecs::codec::high);

			int bufferX(400 / subsample), bufferY(300 / subsample);

			ptr<drawBitmap> testDraw(new drawBitmap(testImage, 0));
			ptr<memory> bitmapBuffer(testDraw->getBitmap<puntoexe::imebra::drawBitmapRGB, 1>(bufferX, bufferY, 0, 0, bufferX, bufferY, 0));
			imbxUint8* pBuffer(bitmapBuffer->data());

			imbxUint32 imageRowSize, imageChannelPixelSize, channels;
			ptr<handlers::dataHandlerNumericBase > pImageHandler(testImage->getDataHandler(false, &imageRowSize, &imageChannelPixelSize, &channels));
			imbxUint8* pImageBuffer (pImageHandler->getMemoryBuffer());

			for(int scanY = 0; scanY != bufferY; ++scanY)
			{
				for(int scanX = 0; scanX != bufferX; ++scanX)
				{
					imbxInt32 averageRed(0), averageGreen(0), averageBlue(0);
					for(int scanSubY(0); scanSubY != subsample; ++scanSubY)
					{
						for(int scanSubX(0); scanSubX != subsample; ++scanSubX)
						{
							imbxUint32 pixel = (scanX * subsample + scanSubX) * channels + (scanY * subsample + scanSubY) * (bufferX * subsample * channels);
							if(monochrome)
							{
								averageRed += pImageBuffer[pixel];
								averageGreen += pImageBuffer[pixel];
								averageBlue += pImageBuffer[pixel];
							}
							else
							{
								averageRed += pImageBuffer[pixel];
								averageGreen += pImageBuffer[++pixel];
								averageBlue += pImageBuffer[++pixel];
							}
						}
					}
					averageRed /= subsample * subsample;
					averageGreen /= subsample * subsample;
					averageBlue /= subsample * subsample;

					imbxUint8 displayRed(*pBuffer++);
					imbxUint8 displayGreen(*pBuffer++);
					imbxUint8 displayBlue(*pBuffer++);
					QVERIFY(averageRed == displayRed);
					QVERIFY(averageGreen == displayGreen);
					QVERIFY(averageBlue == displayBlue);
				}

			}
		}
	}

}


} // namespace tests

} // namespace imebra

} // namespace puntoexe
