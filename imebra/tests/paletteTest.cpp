#include "paletteTest.h"

#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{


// A buffer initialized to a default data type should use the data type OB
void paletteTest::testPalette()
{/*
	imbxUint32 sizeX = 600;
	imbxUint32 sizeY = 400;
	ptr<image> baselineImage(new image);
	baselineImage->create(sizeX, sizeY, image::depthU8, L"RGB", 7);

	imbxUint32 rowSize, channelsPixelSize, channelsNumber;
	ptr<handlers::imageHandler> imageHandler = baselineImage->getDataHandler(true, &rowSize, &channelsPixelSize, &channelsNumber);

	// Make 3 bands (RGB)
	for(imbxUint32 y=0; y<sizeY; ++y)
	{
		for(imbxUint32 x=0; x<sizeX; ++x)
		{
			imbxInt32 r, g, b;
			imbxUint32 value = y * 255 / sizeY;
			r = g = 0;
			b = value;
			if(x < sizeX - sizeX/3)
			{
				r = 0;
				g = value;
				b = 0;
			}
			if(x < sizeX / 3)
			{
				r = value;
				g = 0;
				b = 0;
			}
			imageHandler->setUnsignedLongIncPointer(r);
			imageHandler->setUnsignedLongIncPointer(g);
			imageHandler->setUnsignedLongIncPointer(b);
		}
	}
	imageHandler.release();

	ptr<transforms::colorTransforms::colorTransformsFactory> colorFactory;
	colorFactory = transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory();
	ptr<transforms::transform> colorTransform = colorFactory->getTransform(L"RGB", L"PALETTE COLOR");
	ptr<dataSet> pDataSet(new dataSet);
	colorTransform->declareDataSet(pDataSet);
	colorTransform->declareInputImage(0, baselineImage);
	colorTransform->doTransform();
	ptr<image> paletteImage = colorTransform->getOutputImage(0);

	{
		ptr<transforms::transform> ybrTransform = colorFactory->getTransform(L"PALETTE COLOR", L"YBR_FULL");
		ybrTransform->declareDataSet(pDataSet);
		ybrTransform->declareInputImage(0, paletteImage);
		ybrTransform->doTransform();

		ptr<codecs::jpegCodec> jCodec(new codecs::jpegCodec);
		ptr<stream> outStream(new stream);
		outStream->openFile("c:\\palette.jpg", std::ios::out);
		ptr<streamWriter> writer(new streamWriter(ptr<baseStream>(outStream.get())));
		jCodec->setImage(writer, ybrTransform->getOutputImage(0), L"1.2.840.10008.1.2.4.50", codecs::codec::medium, "OB", 8, false, false, false, false);
	}

	ptr<transforms::transform> colorTransformRev = colorFactory->getTransform(L"PALETTE COLOR", L"RGB");
	colorTransformRev->declareDataSet(pDataSet);
	colorTransformRev->declareInputImage(0, paletteImage);
	colorTransformRev->doTransform();
	ptr<image> rgbImage = colorTransformRev->getOutputImage(0);
	imbxUint32 checkSizeX, checkSizeY;
	rgbImage->getSize(&checkSizeX, &checkSizeY);

	ptr<handlers::imageHandler> rgbHandler = rgbImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);
	ptr<handlers::imageHandler> originalHandler = baselineImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);

	// Compare the buffers. A little difference is allowed
	CPPUNIT_ASSERT(checkSizeX == sizeX);
	CPPUNIT_ASSERT(checkSizeY == sizeY);

	imbxUint32 difference = 0;
	for(imbxUint32 checkY = 0; checkY < sizeY; ++checkY)
	{
		for(imbxUint32 checkX = 0; checkX < sizeX; ++checkX)
		{
			for(imbxUint32 scanChannel = 3; scanChannel != 0; --scanChannel)
			{
				imbxInt32 value0 = rgbHandler->getUnsignedLongIncPointer();
				imbxInt32 value1 = originalHandler->getUnsignedLongIncPointer();
				if(value0 > value1)
				{
					difference += value0 - value1;
				}
				else
				{
					difference += value1 - value0;
				}
			}
		}
	}
	CPPUNIT_ASSERT(difference < sizeX * sizeY * 10);
*/
}



} // namespace tests

} // namespace imebra

} // namespace puntoexe
