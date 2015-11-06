#include "unicodeStringHandlerTest.h"

#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

void unicodeStringHandlerTest::unicodeTest()
{
	ptr<memory> streamMemory(new memory);

	// Try a cyrillic/arabic patient name
	std::wstring patientName0 = L"\x0628\x062a\x062b\x062f^\0x400\0x410\0x420";
	std::wstring patientName1 = L"\0x420\x062a\x062b^\0x400\0x410\x0628\x062a";

	{
		ptr<dataSet> testDataSet(new dataSet);
		charsetsList::tCharsetsList charsets;
		charsets.push_back(L"ISO_IR 192");
		testDataSet->setCharsetsList(&charsets);

		testDataSet->setUnicodeString(0x0010, 0, 0x0010, 0, patientName0);
		testDataSet->setUnicodeString(0x0010, 0, 0x0010, 1, patientName1);

		ptr<baseStream> writeStream(new memoryStream(streamMemory));
		ptr<codecs::dicomCodec> testCodec(new codecs::dicomCodec);
		testCodec->write(ptr<streamWriter>(new streamWriter(writeStream)), testDataSet);
	}

	{
		ptr<baseStream> readStream(new memoryStream(streamMemory));
		ptr<dataSet> testDataSet = codecs::codecFactory::getCodecFactory()->load(ptr<streamReader>(new streamReader(readStream)));

		QCOMPARE(testDataSet->getUnicodeString(0x0010, 0, 0x0010, 0), patientName0);
		QCOMPARE(testDataSet->getUnicodeString(0x0010, 0, 0x0010, 1), patientName1);
		
		charsetsList::tCharsetsList charsets;
		testDataSet->getCharsetsList(&charsets);
		QVERIFY(charsets.size() == 1 && charsets.front()==L"ISO_IR 192");
	}
}

void unicodeStringHandlerTest::iso2022Test()
{
	ptr<memory> streamMemory(new memory);

	// Try a cyrillic /greek/latin patient name
	std::wstring patientName0 = L"\x0430\x0402\x0403\x00c9^\x00d0\x0410\x00d5";
	std::wstring patientName1 = L"\x0420\x00df\x0062^\x0394\x0410\x00d7\x0072";

	{
		ptr<dataSet> testDataSet(new dataSet);

		testDataSet->setUnicodeString(0x0010, 0, 0x0010, 0, patientName0);
		testDataSet->setUnicodeString(0x0010, 0, 0x0010, 1, patientName1);

		ptr<baseStream> writeStream(new memoryStream(streamMemory));
		ptr<codecs::dicomCodec> testCodec(new codecs::dicomCodec);
		testCodec->write(ptr<streamWriter>(new streamWriter(writeStream)), testDataSet);
	}

	{
		ptr<baseStream> readStream(new memoryStream(streamMemory));
		ptr<dataSet> testDataSet = codecs::codecFactory::getCodecFactory()->load(ptr<streamReader>(new streamReader(readStream)));

		std::wstring check0 = testDataSet->getUnicodeString(0x0010, 0, 0x0010, 0);
		std::wstring check1 = testDataSet->getUnicodeString(0x0010, 0, 0x0010, 1);
		QCOMPARE(check0, patientName0);
		QCOMPARE(check1, patientName1);
		
		charsetsList::tCharsetsList charsets;
		testDataSet->getCharsetsList(&charsets);
		QVERIFY(charsets.size() == 4 && charsets.front()==L"ISO 2022 IR 6");
	}
}

} // namespace tests

} // namespace imebra

} // namespace puntoexe
