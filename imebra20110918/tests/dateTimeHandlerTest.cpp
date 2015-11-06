#include "dateTimeHandlerTest.h"
#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

void dateTimeHandlerTest::dateTest()
{
	ptr<data> tag(new data(ptr<baseObject>(0)));
	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, true, "DA");
		hTag->setSize(1);

		hTag->setDate(0, 2004, 11, 5, 9, 20, 30, 5000, 1, 2);

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 2004);
		QVERIFY(month == 11);
		QVERIFY(day == 5);
		QVERIFY(hour == 0);
		QVERIFY(minutes == 0);
		QVERIFY(seconds == 0);
		QVERIFY(nanoseconds == 0);
		QVERIFY(offsetHours == 0);
		QVERIFY(offsetMinutes == 0);

		QVERIFY(hTag->getUnicodeString(0) == L"2004-11-05");
	}

	{
		ptr<handlers::dataHandlerRaw> hTag= tag->getDataHandlerRaw(0, true, "DA");
		std::basic_string<imbxUint8> checkString(hTag->getMemory()->data(), hTag->getMemory()->size());
		QVERIFY(checkString == (imbxUint8*)"20041105");
		hTag->getMemory()->assign((imbxUint8*)"2004-11-5", 9);
	}

	{
		ptr<handlers::dataHandlerRaw> hTag= tag->getDataHandlerRaw(0, false, "DA");
		std::basic_string<imbxUint8> checkString(hTag->getMemory()->data(), hTag->getMemory()->size());
		stringUint8 compString((imbxUint8*)"2004-11-5", 9);
		compString += (imbxUint8)0; // buffer's size is always even!
		QVERIFY(checkString == compString);
	}

	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, false, "DA");

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 2004);
		QVERIFY(month == 11);
		QVERIFY(day == 5);
		QVERIFY(hour == 0);
		QVERIFY(minutes == 0);
		QVERIFY(seconds == 0);
		QVERIFY(nanoseconds == 0);
		QVERIFY(offsetHours == 0);
		QVERIFY(offsetMinutes == 0);

		QVERIFY(hTag->getUnicodeString(0) == L"2004-11-05");
	}
}

void dateTimeHandlerTest::timeTest()
{
	ptr<data> tag(new data(ptr<baseObject>(0)));
	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, true, "TM");
		hTag->setSize(1);

		hTag->setDate(0, 2004, 11, 5, 9, 20, 40, 5000, 1, 2);

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 0);
		QVERIFY(month == 0);
		QVERIFY(day == 0);
		QVERIFY(hour == 9);
		QVERIFY(minutes == 20);
		QVERIFY(seconds == 40);
		QVERIFY(nanoseconds == 5000);
		QVERIFY(offsetHours == 0);
		QVERIFY(offsetMinutes == 0);

		QVERIFY(hTag->getUnicodeString(0) == L"09:20:40.005000");
	}

	{
		ptr<handlers::dataHandlerRaw> hTag= tag->getDataHandlerRaw(0, true, "TM");
		stringUint8 compString((imbxUint8*)"092040.005000");
		compString += (imbxUint8)0;
		std::basic_string<imbxUint8> checkString(hTag->getMemory()->data(), hTag->getMemory()->size());
		QVERIFY(checkString == compString);
		hTag->getMemory()->assign((imbxUint8*)"9:20:40", 7);
	}

	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, false, "TM");

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 0);
		QVERIFY(month == 0);
		QVERIFY(day == 0);
		QVERIFY(hour == 9);
		QVERIFY(minutes == 20);
		QVERIFY(seconds == 40);
		QVERIFY(nanoseconds == 0);
		QVERIFY(offsetHours == 0);
		QVERIFY(offsetMinutes == 0);

		QVERIFY(hTag->getUnicodeString(0) == L"09:20:40.000000");
	}
}

void dateTimeHandlerTest::dateTimeTest()
{
	ptr<data> tag(new data(ptr<baseObject>(0)));
	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, true, "DT");
		hTag->setSize(1);

		hTag->setDate(0, 2004, 11, 5, 9, 20, 40, 5000, 1, 2);

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 2004);
		QVERIFY(month == 11);
		QVERIFY(day == 5);
		QVERIFY(hour == 9);
		QVERIFY(minutes == 20);
		QVERIFY(seconds == 40);
		QVERIFY(nanoseconds == 5000);
		QVERIFY(offsetHours == 1);
		QVERIFY(offsetMinutes == 2);

		QVERIFY(hTag->getUnicodeString(0) == L"2004-11-05 09:20:40.005000+01:02");
	}

	{
		ptr<handlers::dataHandlerRaw> hTag= tag->getDataHandlerRaw(0, false, "DT");
		std::basic_string<imbxUint8> checkString(hTag->getMemory()->data(), hTag->getMemory()->size());
		QVERIFY(checkString == (imbxUint8*)"20041105092040.005000+0102");
	}

	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, true, "DT");

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 2004);
		QVERIFY(month == 11);
		QVERIFY(day == 5);
		QVERIFY(hour == 9);
		QVERIFY(minutes == 20);
		QVERIFY(seconds == 40);
		QVERIFY(nanoseconds == 5000);
		QVERIFY(offsetHours == 1);
		QVERIFY(offsetMinutes == 2);

		QVERIFY(hTag->getUnicodeString(0) == L"2004-11-05 09:20:40.005000+01:02");

		hTag->setString(0, "2005-12-06 10:21:41.005001-4:5");
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);
		QVERIFY(year == 2005);
		QVERIFY(month == 12);
		QVERIFY(day == 6);
		QVERIFY(hour == 10);
		QVERIFY(minutes == 21);
		QVERIFY(seconds == 41);
		QVERIFY(nanoseconds == 5001);
		QVERIFY(offsetHours == -4);
		QVERIFY(offsetMinutes == -5);
	}

	{
		ptr<handlers::dataHandlerRaw> hTag= tag->getDataHandlerRaw(0, true, "DT");
		std::basic_string<imbxUint8> checkString(hTag->getMemory()->data(), hTag->getMemory()->size());
		QVERIFY(checkString == (imbxUint8*)"20051206102141.005001-0405");
		hTag->getMemory()->assign((imbxUint8*)"19990305", 8);
	}

	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, false, "DT");

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 1999);
		QVERIFY(month == 03);
		QVERIFY(day == 05);
		QVERIFY(hour == 0);
		QVERIFY(minutes == 0);
		QVERIFY(seconds == 0);
		QVERIFY(nanoseconds == 0);
		QVERIFY(offsetHours == 0);
		QVERIFY(offsetMinutes == 0);
	}

	{
		ptr<handlers::dataHandlerRaw> hTag= tag->getDataHandlerRaw(0, true, "DT");
		hTag->getMemory()->assign((imbxUint8*)"1999030508", 10);
	}

	{
		ptr<handlers::dataHandler> hTag= tag->getDataHandler(0, false, "DT");

		imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
		hTag->getDate(0, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

		QVERIFY(year == 1999);
		QVERIFY(month == 03);
		QVERIFY(day == 05);
		QVERIFY(hour == 8);
		QVERIFY(minutes == 0);
		QVERIFY(seconds == 0);
		QVERIFY(nanoseconds == 0);
		QVERIFY(offsetHours == 0);
		QVERIFY(offsetMinutes == 0);
	}
}



} // namespace tests

} // namespace imebra

} // namespace puntoexe
