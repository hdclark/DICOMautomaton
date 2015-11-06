#include "jpegCodecTest.h"

#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

// A buffer initialized to a default data type should use the data type OB
void jpegCodecTest::testBaseline()
{
	for(int precision=0; precision != 2; ++precision)
	{
		ptr<imebra::dataSet> dataset(new imebra::dataSet);

		imbxUint32 sizeX = 600;
		imbxUint32 sizeY = 400;
		ptr<image> baselineImage(new image);
		baselineImage->create(sizeX, sizeY, precision == 0 ? image::depthU8 : image::depthU16, L"RGB", precision == 0 ? 7 : 11);

		imbxUint32 rowSize, channelsPixelSize, channelsNumber;
		ptr<handlers::dataHandlerNumericBase> imageHandler = baselineImage->getDataHandler(true, &rowSize, &channelsPixelSize, &channelsNumber);

		// Make 3 bands (RGB)
		int elementPointer(0);
		for(imbxUint32 y=0; y<sizeY; ++y)
		{
			for(imbxUint32 x=0; x<sizeX; ++x)
			{
				imbxInt32 r, g, b;
				imbxUint32 value = y * (precision == 0 ? 255 : 4095) / sizeY;
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
				imageHandler->setUnsignedLong(elementPointer++, r);
				imageHandler->setUnsignedLong(elementPointer++, g);
				imageHandler->setUnsignedLong(elementPointer++, b);
			}
		}
		imageHandler.release();

		ptr<transforms::colorTransforms::colorTransformsFactory> colorFactory;
		colorFactory = transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory();
		ptr<transforms::colorTransforms::colorTransform> colorTransform = colorFactory->getTransform(L"RGB", L"YBR_FULL");
		ptr<image> ybrImage = colorTransform->allocateOutputImage(baselineImage, sizeX, sizeY);
		colorTransform->runTransform(baselineImage, 0, 0, sizeX, sizeY, ybrImage, 0, 0);

		std::wstring fileName;
		if(precision == 0)
		{
			fileName = L"testDicomLossyJpeg8bit.dcm";
			dataset->setImage(0, ybrImage, L"1.2.840.10008.1.2.4.50", codecs::codec::veryHigh);
		}
		else
		{
			fileName = L"testDicomLossyJpeg12bit.dcm";
			dataset->setImage(0, ybrImage, L"1.2.840.10008.1.2.4.51", codecs::codec::veryHigh);
		}

		ptr<imebra::codecs::dicomCodec> saveDicom(new imebra::codecs::dicomCodec);
		ptr<puntoexe::stream> saveDicomStream(new puntoexe::stream);
		saveDicomStream->openFile(fileName, std::ios_base::out | std::ios_base::trunc);
		ptr<puntoexe::streamWriter> saveDicomStreamWriter(new puntoexe::streamWriter(saveDicomStream));
		saveDicom->write(saveDicomStreamWriter, dataset);
		saveDicomStreamWriter.release();
		saveDicomStream.release();

		ptr<image> checkImage = dataset->getImage(0);
		imbxUint32 checkSizeX, checkSizeY;
		checkImage->getSize(&checkSizeX, &checkSizeY);

		colorTransform = colorFactory->getTransform(L"YBR_FULL", L"RGB");
		ptr<image> rgbImage = colorTransform->allocateOutputImage(checkImage, checkSizeX, checkSizeY);
		colorTransform->runTransform(checkImage, 0, 0, checkSizeX, checkSizeY, rgbImage, 0, 0);
		ptr<handlers::dataHandlerNumericBase> rgbHandler = rgbImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);
		ptr<handlers::dataHandlerNumericBase> originalHandler = baselineImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);

		// Compare the buffers. A little difference is allowed
		QCOMPARE(checkSizeX, sizeX);
		QCOMPARE(checkSizeY, sizeY);

		elementPointer = 0;

		imbxUint32 difference = 0;
		for(imbxUint32 checkY = 0; checkY < sizeY; ++checkY)
		{
			for(imbxUint32 checkX = 0; checkX < sizeX; ++checkX)
			{
				for(imbxUint32 channel = 3; channel != 0; --channel)
				{
					imbxInt32 value0 = rgbHandler->getUnsignedLong(elementPointer);
					imbxInt32 value1 = originalHandler->getUnsignedLong(elementPointer++);
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
		QVERIFY(difference < sizeX * sizeY);
	}
}


void jpegCodecTest::testBaselineSubsampled()
{
	imbxUint32 sizeX = 600;
	imbxUint32 sizeY = 400;
	ptr<image> baselineImage(new image);
	baselineImage->create(sizeX, sizeY, image::depthU8, L"RGB", 7);

	imbxUint32 rowSize, channelsPixelSize, channelsNumber;
	ptr<handlers::dataHandlerNumericBase> imageHandler = baselineImage->getDataHandler(true, &rowSize, &channelsPixelSize, &channelsNumber);

	// Make 3 bands (RGB)
	imbxUint32 elementNumber(0);
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
			imageHandler->setUnsignedLong(elementNumber++, r);
			imageHandler->setUnsignedLong(elementNumber++, g);
			imageHandler->setUnsignedLong(elementNumber++, b);
		}
	}
	imageHandler.release();

	ptr<transforms::colorTransforms::colorTransformsFactory> colorFactory;
	colorFactory = transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory();
	ptr<transforms::colorTransforms::colorTransform> colorTransform = colorFactory->getTransform(L"RGB", L"YBR_FULL");
	ptr<image> ybrImage = colorTransform->allocateOutputImage(baselineImage, sizeX, sizeY);
	colorTransform->runTransform(baselineImage, 0, 0, sizeX, sizeY, ybrImage, 0, 0);

	ptr<memory> streamMemory(new memory);
	{
		ptr<baseStream> writeStream(new memoryStream(streamMemory));
		ptr<streamWriter> writer(new streamWriter(writeStream));

		ptr<codecs::jpegCodec> testCodec(new codecs::jpegCodec);
		testCodec->setImage(writer, ybrImage, L"1.2.840.10008.1.2.4.50", codecs::codec::medium, "OB", 8, true, true, false, false);
	}

	ptr<baseStream> readStream(new memoryStream(streamMemory));
	ptr<streamReader> reader(new streamReader(readStream));
		
	ptr<codecs::jpegCodec> testCodec(new codecs::jpegCodec);
	ptr<dataSet> readDataSet = testCodec->read(reader);
	ptr<image> checkImage = readDataSet->getImage(0);
	imbxUint32 checkSizeX, checkSizeY;
	checkImage->getSize(&checkSizeX, &checkSizeY);

	colorTransform = colorFactory->getTransform(L"YBR_FULL", L"RGB");
	ptr<image> rgbImage = colorTransform->allocateOutputImage(checkImage, checkSizeX, checkSizeY);
	colorTransform->runTransform(checkImage, 0, 0, checkSizeX, checkSizeY, rgbImage, 0, 0);
	ptr<handlers::dataHandlerNumericBase> rgbHandler = rgbImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);
	ptr<handlers::dataHandlerNumericBase> originalHandler = baselineImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);

	// Compare the buffers. A little difference is allowed
	QCOMPARE(checkSizeX, sizeX);
	QCOMPARE(checkSizeY, sizeY);

	imbxUint32 difference = 0;
	elementNumber = 0;
	for(imbxUint32 checkY = 0; checkY < sizeY; ++checkY)
	{
		for(imbxUint32 checkX = 0; checkX < sizeX; ++checkX)
		{
			for(imbxUint32 channel = 3; channel != 0; --channel)
			{
				imbxInt32 value0 = rgbHandler->getUnsignedLong(elementNumber);
				imbxInt32 value1 = originalHandler->getUnsignedLong(elementNumber++);
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
	QVERIFY(difference < sizeX * sizeY * 12);

}

void jpegCodecTest::testLossless()
{
	imbxUint32 sizeX = 115;
	imbxUint32 sizeY = 400;
	ptr<image> baselineImage(new image);
	baselineImage->create(sizeX, sizeY, image::depthU8, L"RGB", 7);

	imbxUint32 rowSize, channelsPixelSize, channelsNumber;
	ptr<handlers::dataHandlerNumericBase> imageHandler = baselineImage->getDataHandler(true, &rowSize, &channelsPixelSize, &channelsNumber);

	// Make 3 bands (RGB)
	imbxUint32 elementNumber(0);
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
			imageHandler->setUnsignedLong(elementNumber++, r);
			imageHandler->setUnsignedLong(elementNumber++, g);
			imageHandler->setUnsignedLong(elementNumber++, b);
		}
	}
	imageHandler.release();

	ptr<memory> streamMemory(new memory);
	{
		ptr<baseStream> writeStream(new memoryStream(streamMemory));
		ptr<streamWriter> writer(new streamWriter(writeStream));
		ptr<codecs::jpegCodec> testCodec(new codecs::jpegCodec);
		testCodec->setImage(writer, baselineImage, L"1.2.840.10008.1.2.4.57", codecs::codec::medium, "OB", 8, false, false, false, false);
	}

	ptr<baseStream> readStream(new memoryStream(streamMemory));
	ptr<streamReader> reader(new streamReader(readStream));
		
	ptr<codecs::jpegCodec> testCodec(new codecs::jpegCodec);
	ptr<dataSet> readDataSet = testCodec->read(reader);
	ptr<image> checkImage = readDataSet->getImage(0);
	imbxUint32 checkSizeX, checkSizeY;
	checkImage->getSize(&checkSizeX, &checkSizeY);

	ptr<handlers::dataHandlerNumericBase> rgbHandler = checkImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);
	ptr<handlers::dataHandlerNumericBase> originalHandler = baselineImage->getDataHandler(false, &rowSize, &channelsPixelSize, &channelsNumber);

	// Compare the buffers. A little difference is allowed
	QCOMPARE(checkSizeX, sizeX);
	QCOMPARE(checkSizeY, sizeY);

	elementNumber = 0;

	for(imbxUint32 checkY = 0; checkY < sizeY; ++checkY)
	{
		for(imbxUint32 checkX = 0; checkX < sizeX; ++checkX)
		{
			for(imbxUint32 channel = 3; channel != 0; --channel)
			{
				imbxInt32 value0 = rgbHandler->getUnsignedLong(elementNumber);
				imbxInt32 value1 = originalHandler->getUnsignedLong(elementNumber++);
				QCOMPARE(value0, value1);
			}
		}
	}

}


} // namespace tests

} // namespace imebra

} // namespace puntoexe
