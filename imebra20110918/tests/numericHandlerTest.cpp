#include "numericHandlerTest.h"

#include "../library/imebra/include/imebra.h"
#include <string.h>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

// Check the function pointerIsValid
void numericHandlerTest::validPointer()
{
	ptr<buffer> buffer0(new buffer(0, "OW"));


	ptr<handlers::dataHandler> handlerBuffer0(buffer0->getDataHandler(true, 0));
	QVERIFY(handlerBuffer0->getUnitSize() == 2);
	imbxUint32 bufferSize(4);
	handlerBuffer0->setSize(bufferSize);
	
	imbxUint32 checkSize0(0);
	while(handlerBuffer0->pointerIsValid(checkSize0))
	{
		QVERIFY(checkSize0 < bufferSize);
		++checkSize0;
	}

	QCOMPARE(checkSize0, bufferSize);
	handlerBuffer0.release();

	ptr<handlers::dataHandler> handlerBuffer1(buffer0->getDataHandler(true, 0));
	QVERIFY(handlerBuffer1->getUnitSize() == 2);
	
	imbxUint32 checkSize1(0);
	while(handlerBuffer1->pointerIsValid(checkSize1))
	{
		QVERIFY(checkSize1 < bufferSize);
		++checkSize1;
	}

	QCOMPARE(checkSize1, bufferSize);
}

// A buffer initialized to a default data type should use the data type OB
void numericHandlerTest::interleavedCopy()
{
	int sizeX = 601;
	int sizeY = 401;
	ptr<image> testImage(new image);
	testImage->create(sizeX, sizeY, image::depthS16, L"RGB", 7);
	imbxUint32 rowSize, channelSize, channelsNumber;
	ptr<handlers::dataHandlerNumericBase> testHandler = testImage->getDataHandler(true, &rowSize, &channelSize, &channelsNumber);

	size_t pointer(0);
	for(int y = 0; y < sizeY; ++y)
	{
		for(int x = 0; x < sizeX; ++x)
		{
			int squareX = x >> 3;
			int squareY = y >> 3;

			int squareX2 = squareX >> 1;
			int squareY2 = squareY >> 1;

			testHandler->setSignedLong(pointer++, squareY * 100 + squareX);
			testHandler->setSignedLong(pointer++, -squareY2 * 100 - squareX2);
			testHandler->setSignedLong(pointer++, -squareY2 * 100 + squareX2);
		}
	}

	int mcuX = (sizeX + 15) >> 4;
	int mcuY = (sizeY + 15) >> 4;

	std::auto_ptr<imbxInt32> buffer0(new imbxInt32[mcuX * 16 * mcuY * 16]);
	::memset(buffer0.get(), 0, mcuX * 16 * mcuY * 16 * sizeof(imbxInt32));
	std::auto_ptr<imbxInt32> buffer1(new imbxInt32[mcuX * 8 * mcuY * 8]);
	::memset(buffer1.get(), 0, mcuX * 8 * mcuY * 8 * sizeof(imbxInt32));
	std::auto_ptr<imbxInt32> buffer2(new imbxInt32[mcuX * 8 * mcuY * 8]);
	::memset(buffer2.get(), 0, mcuX * 8 * mcuY * 8 * sizeof(imbxInt32));

	for(int scanMcuY = 0; scanMcuY < mcuY; ++scanMcuY)
	{
		for(int scanMcuX = 0; scanMcuX < mcuX; ++scanMcuX)
		{
			testHandler->copyToInt32Interleaved(
				buffer0.get() + (128*scanMcuX) + (256*mcuX*scanMcuY),
				1,
				1,
				scanMcuX * 16,
				scanMcuY * 16,
				scanMcuX * 16 + 8,
				scanMcuY * 16 + 8,
				0,
				sizeX,
				sizeY,
				3);
			
			testHandler->copyToInt32Interleaved(
				buffer0.get() + 64 + (128*scanMcuX) + (256*mcuX*scanMcuY),
				1,
				1,
				scanMcuX * 16 + 8,
				scanMcuY * 16,
				scanMcuX * 16 + 16,
				scanMcuY * 16 + 8,
				0,
				sizeX,
				sizeY,
				3);
			
			testHandler->copyToInt32Interleaved(
				buffer0.get() + (128*scanMcuX) + (256*mcuX*scanMcuY) + 128 * mcuX,
				1,
				1,
				scanMcuX * 16,
				scanMcuY * 16 + 8,
				scanMcuX * 16 + 8,
				scanMcuY * 16 + 16,
				0,
				sizeX,
				sizeY,
				3);
			
			testHandler->copyToInt32Interleaved(
				buffer0.get() + 64 + (128*scanMcuX) + (256*mcuX*scanMcuY) + 128 * mcuX,
				1,
				1,
				scanMcuX * 16 + 8,
				scanMcuY * 16 + 8,
				scanMcuX * 16 + 16,
				scanMcuY * 16 + 16,
				0,
				sizeX,
				sizeY,
				3);
			
			
			testHandler->copyToInt32Interleaved(
				buffer1.get() + (64*scanMcuX) + (64*mcuX*scanMcuY),
				2,
				2,
				scanMcuX * 16,
				scanMcuY * 16,
				scanMcuX * 16 + 16,
				scanMcuY * 16 + 16,
				1,
				sizeX,
				sizeY,
				3);
			
			testHandler->copyToInt32Interleaved(
				buffer2.get() + (64*scanMcuX) + (64*mcuX*scanMcuY),
				2,
				2,
				scanMcuX * 16,
				scanMcuY * 16,
				scanMcuX * 16 + 16,
				scanMcuY * 16 + 16,
				2,
				sizeX,
				sizeY,
				3);
		
		}
	}

	for(int y = 0; y < sizeY; y+=8)
	{
		for(int x = 0; x < sizeX; x+=8)
		{
			int squareX = x >> 3;
			int squareY = y >> 3;

			int squareX2 = squareX >> 1;
			int squareY2 = squareY >> 1;

			imbxInt32* pTestBuffer0 = buffer0.get() + squareX * 64 + squareY * mcuX * 128;
			for(int scanValues0 = 0; scanValues0 < 64; ++scanValues0)
			{
				QCOMPARE(pTestBuffer0[scanValues0], (imbxInt32)(squareY * 100 + squareX) );
			}

			imbxInt32* pTestBuffer1 = buffer1.get() + squareX2 * 64 + squareY2 * mcuX * 64;
			for(int scanValues1 = 0; scanValues1 < 64; ++scanValues1)
			{
				QCOMPARE(pTestBuffer1[scanValues1], (imbxInt32)(-squareY2 * 100 - squareX2) );
			}

			imbxInt32* pTestBuffer2 = buffer2.get() + squareX2 * 64 + squareY2 * mcuX * 64;
			for(int scanValues2 = 0; scanValues2 < 64; ++scanValues2)
			{
				QCOMPARE(pTestBuffer2[scanValues2], (imbxInt32)(-squareY2 * 100 + squareX2) );
			}

		}
	}

}


void numericHandlerTest::stringConversion()
{
	ptr<memory> handlerBuffer0(new memory);
	ptr<handlers::dataHandlerNumeric<imbxInt32> > testHandler0(new handlers::dataHandlerNumeric<imbxInt32>);
	testHandler0->parseBuffer(handlerBuffer0);
	testHandler0->setSize(1);

	testHandler0->setString(0, "13");
	QVERIFY(testHandler0->getSignedLong(0) == 13);
	testHandler0->setString(0, "45.7");
	QVERIFY(testHandler0->getSignedLong(0) == 45);

	ptr<memory> handlerBuffer1(new memory);
	ptr<handlers::dataHandlerNumeric<double> > testHandler1(new handlers::dataHandlerNumeric<double>);
	testHandler1->parseBuffer(handlerBuffer1);
	testHandler1->setSize(1);

	testHandler1->setString(0, "13");
	QVERIFY(testHandler1->getSignedLong(0) == 13);
	testHandler1->setString(0, "45.7");
	QVERIFY(testHandler1->getDouble(0) == 45.7);
}



} // namespace tests

} // namespace imebra

} // namespace puntoexe
