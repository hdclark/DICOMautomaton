#include "ageStringHandlerTest.h"
#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

void ageStringHandlerTest::ageTest()
{
	ptr<data> tag(new data(ptr<baseObject>(0)));
	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, true, "AS");
		hTag->setSize(1);

		hTag->setDouble(0, 0.01);
		std::wstring ageString = hTag->getUnicodeString(0);
		QCOMPARE(ageString, std::wstring(L"003D"));

		hTag->setDouble(0, 0.2);
		ageString = hTag->getUnicodeString(0);
		QVERIFY(ageString == L"010W");

		hTag->setDouble(0, 0.9);
		ageString = hTag->getUnicodeString(0);
		QVERIFY(ageString == L"010M");

		hTag->setDouble(0, 0.5);
		ageString = hTag->getUnicodeString(0);
		QVERIFY(ageString == L"006M");

		hTag->setDouble(0, 2.3);
		ageString = hTag->getUnicodeString(0);
		QVERIFY(ageString == L"002Y");

		imbxUint32 ageInt = hTag->getUnsignedLong(0);
		QVERIFY(ageInt == 2);
	}
}



} // namespace tests

} // namespace imebra

} // namespace puntoexe
