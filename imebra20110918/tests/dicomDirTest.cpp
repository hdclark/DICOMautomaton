#include "dicomDirTest.h"

#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

using namespace puntoexe::imebra;

void dicomDirTest::createDicomDir()
{
	ptr<dicomDir> newDicomDir(new dicomDir(new dataSet));

	ptr<directoryRecord> pRootRecord(newDicomDir->getNewRecord());
	pRootRecord->getRecordDataSet()->setUnicodeString(0x10, 0, 0x10, 0, L"Surname");
	pRootRecord->setType(directoryRecord::patient);
	newDicomDir->setFirstRootRecord(pRootRecord);

	ptr<directoryRecord> pNextRecord(newDicomDir->getNewRecord());
	pNextRecord->getRecordDataSet()->setUnicodeString(0x10, 0, 0x10, 0, L"Surname 1");
	pNextRecord->setType(directoryRecord::patient);
	pRootRecord->setNextRecord(pNextRecord);

	ptr<directoryRecord> pImageRecord(newDicomDir->getNewRecord());
	pImageRecord->getRecordDataSet()->setUnicodeString(0x8, 0, 0x18, 0, L"1.2.840.34.56.78999654.235");
	pImageRecord->setType(directoryRecord::image);
        pImageRecord->setFilePart(0, L"folder");
        pImageRecord->setFilePart(1, L"file.dcm");

	pNextRecord->setFirstChildRecord(pImageRecord);

	ptr<dataSet> dicomDirDataSet(newDicomDir->buildDataSet());

	ptr<memory> streamMemory(new memory);
	ptr<memoryStream> memStream(new memoryStream(streamMemory));
	ptr<streamWriter> memWriter(new streamWriter(memStream));
	ptr<imebra::codecs::dicomCodec> dicomWriter(new imebra::codecs::dicomCodec);
	dicomWriter->write(memWriter, dicomDirDataSet);
	memWriter.release();

	ptr<streamReader> memReader(new streamReader(memStream));
	ptr<imebra::codecs::dicomCodec> dicomReader(new imebra::codecs::dicomCodec);
	ptr<dataSet> readDataSet(dicomReader->read(memReader));

	ptr<dicomDir> testDicomDir(new dicomDir(readDataSet));
	ptr<directoryRecord> testRootRecord(testDicomDir->getFirstRootRecord());
	QVERIFY(testRootRecord->getType() == directoryRecord::patient);
	QVERIFY(testRootRecord->getRecordDataSet()->getUnicodeString(0x10, 0, 0x10, 0) == L"Surname");

	ptr<directoryRecord> testNextRecord(testRootRecord->getNextRecord());
	QVERIFY(testNextRecord->getType() == directoryRecord::patient);
	QVERIFY(testNextRecord->getRecordDataSet()->getUnicodeString(0x10, 0, 0x10, 0) == L"Surname 1");

	ptr<directoryRecord> testImageRecord(testNextRecord->getFirstChildRecord());
	QVERIFY(testImageRecord->getType() == directoryRecord::image);
	QVERIFY(testImageRecord->getRecordDataSet()->getUnicodeString(0x8, 0, 0x18, 0) == L"1.2.840.34.56.78999654.235");
	QVERIFY(testImageRecord->getFilePart(0) == L"folder");
	QVERIFY(testImageRecord->getFilePart(1) == L"file.dcm");
}


} // namespace tests

} // namespace imebra

} // namespace puntoexe
