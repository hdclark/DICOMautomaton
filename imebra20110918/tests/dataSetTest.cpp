#include "dataSetTest.h"
#include "../library/imebra/include/imebra.h"
#include "buildImageForTest.h"
#include <list>
#include <string.h>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

using namespace puntoexe::imebra;

void dataSetTest::testFragmentation()
{
    // Add two images to a dataset, then fragment the first image
	ptr<image> testImage0(buildImageForTest(
		400, 
		300, 
		puntoexe::imebra::image::depthU8,
		7, 
		400, 
		300, 
		L"RGB", 
		50));

	ptr<image> testImage1(buildImageForTest(
		400, 
		300, 
		puntoexe::imebra::image::depthU8,
		7, 
		400, 
		300, 
		L"RGB", 
		20));

	ptr<dataSet> testDataSet(new dataSet);
	testDataSet->setImage(0, testImage0, L"1.2.840.10008.1.2.4.70", codecs::codec::high);
	testDataSet->setImage(1, testImage1, L"1.2.840.10008.1.2.4.70", codecs::codec::high);

	ptr<image> verifyImage0 = testDataSet->getImage(0);
        QVERIFY(compareImages(testImage0, verifyImage0) < 0.000001);
	ptr<image> verifyImage1 = testDataSet->getImage(1);
        QVERIFY(compareImages(testImage1, verifyImage1) < 0.000001);

	// Now defragment the stored buffer
	ptr<data> imageTag = testDataSet->getTag(0x7fe0, 0, 0x0010, false);
        QVERIFY(imageTag != 0);

	std::list<ptr<buffer> > newBuffers;
	ptr<buffer> newTableOffsetBuffer(new buffer(testDataSet));
	newBuffers.push_back(newTableOffsetBuffer);
	imbxUint32 offset(0);

	for(imbxUint32 scanBuffers = 1; imageTag->bufferExists(scanBuffers); ++scanBuffers)
	{
		ptr<handlers::dataHandlerRaw> offsetHandler = newTableOffsetBuffer->getDataHandlerRaw(true, 8);
		imbxUint32* pOffsetMemory = (imbxUint32*)(offsetHandler->getMemoryBuffer());
		pOffsetMemory[scanBuffers - 1] = offset;
		streamController::adjustEndian((imbxUint8*)&(pOffsetMemory[scanBuffers - 1]), sizeof(pOffsetMemory[0]), streamController::lowByteEndian, 1);
		ptr<handlers::dataHandlerRaw> wholeHandler = imageTag->getDataHandlerRaw(scanBuffers, false, "");
		imbxUint8* pWholeHandler = (imbxUint8*)wholeHandler->getMemoryBuffer();
		imbxUint32 totalSize = wholeHandler->getSize();
		imbxUint32 fragmentedSize = totalSize / 3;
		if(fragmentedSize & 0x1)
		{
			++fragmentedSize;
		}
		while(totalSize != 0)
		{
			imbxUint32 thisSize = totalSize;
			if(thisSize > fragmentedSize)
			{
				thisSize = fragmentedSize;
			}
			ptr<buffer> newBuffer(new buffer(ptr<baseObject>(0), "OB", ptr<baseStream>(0), 0, thisSize, 1, streamController::lowByteEndian) );
			ptr<handlers::dataHandlerRaw> newBufferHandler = newBuffer->getDataHandlerRaw(true, thisSize);
			imbxUint8* pNewBuffer = (imbxUint8*)newBufferHandler->getMemoryBuffer();
			::memcpy(pNewBuffer, pWholeHandler, thisSize);
			newBufferHandler.release();
			newBuffers.push_back(newBuffer);
			offset += newBuffer->getBufferSizeBytes() + 8;
			totalSize -= thisSize;
			pWholeHandler += thisSize;
		}
	}
        
	imbxUint32 bufferId(0);
	for(std::list<ptr<buffer> >::const_iterator addBuffers = newBuffers.begin(); addBuffers != newBuffers.end(); ++addBuffers)
	{
		imageTag->setBuffer(bufferId++, *addBuffers);
	}

	ptr<image> compareImage0 = testDataSet->getImage(0);
        QVERIFY(compareImages(testImage0, compareImage0) < 0.000001);
	ptr<image> compareImage1 = testDataSet->getImage(1);
        QVERIFY(compareImages(testImage1, compareImage1) < 0.000001);
        QVERIFY(compareImages(testImage0, compareImage1) > 30);
}

void dataSetTest::testSetTagTwice()
{
    ptr<dataSet> testDataset(new dataSet);

    IMEBRA_TRANSACTION_START();

    testDataset->setUnicodeString(0x0008, 0x0, 0x0070, 0x0, L"Puntoexe");
    testDataset->setUnicodeString(0x0008, 0x0, 0x0070, 0x0, L"Puntoexe");

    IMEBRA_TRANSACTION_END();

    QVERIFY(testDataset->getUnicodeString(0x0008, 0x0, 0x0070, 0x0) == L"Puntoexe");


    IMEBRA_TRANSACTION_START();

    testDataset->setUnicodeString(0x0008, 0x0, 0x0070, 0x0, L"Puntoexe");

    IMEBRA_TRANSACTION_START();
    testDataset->setUnicodeString(0x0008, 0x0, 0x0070, 0x0, L"Puntoexe");
    IMEBRA_TRANSACTION_END();

    IMEBRA_TRANSACTION_END();

    QVERIFY(testDataset->getUnicodeString(0x0008, 0x0, 0x0070, 0x0) == L"Puntoexe");

}


} // namespace tests

} // namespace imebra

} // namespace puntoexe
