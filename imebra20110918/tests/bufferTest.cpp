#include "bufferTest.h"
#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

void bufferTest::testDefaultType()
{
	ptr<buffer> patientBuffer(new buffer(ptr<baseObject>(0)));
	QVERIFY(patientBuffer->getDataType() == "OB");
}

void bufferTest::testReadWrite()
{
	ptr<buffer> patientBuffer(new buffer(ptr<baseObject>(0), "UL"));

	// get a data reading handler
	ptr<handlers::dataHandler> readingHandler0 = patientBuffer->getDataHandler(false);

	// get a writing handler and write something
	{
		ptr<handlers::dataHandler> writingHandler0 = patientBuffer->getDataHandler(true);
		writingHandler0->setSize(10);
		for(int writeNumbers = 0; writeNumbers < 10; ++writeNumbers)
		{
			writingHandler0->setSignedLong(writeNumbers, writeNumbers);
		}
	}

	// now, the first reading handler should still be empty
        QVERIFY(readingHandler0->getSize() == 0);

	// Get a new reading handler
	ptr<handlers::dataHandler> readingHandler1 = patientBuffer->getDataHandler(false);

	// Get two different writing handlers
	ptr<handlers::dataHandler> writingHandler1 = patientBuffer->getDataHandler(true);
	ptr<handlers::dataHandler> writingHandler2 = patientBuffer->getDataHandler(true);

	// Get another reading handler
	ptr<handlers::dataHandler> readingHandler2 = patientBuffer->getDataHandler(false);

	// Check the values in all the new handlers first
		for(imbxInt32 checkValues = 0; checkValues < 10; ++checkValues)
	{
                QCOMPARE(writingHandler1->getSignedLong(checkValues), checkValues);
                QCOMPARE(writingHandler2->getSignedLong(checkValues), checkValues);
                QCOMPARE(readingHandler1->getSignedLong(checkValues), checkValues);
                QCOMPARE(readingHandler2->getSignedLong(checkValues), checkValues);
	}

	// Change the content in the first writing handler
	writingHandler1->setSize(20);
        QVERIFY(writingHandler1->getSize() == 20);
	for(int writeValues = 0; writeValues < 20; ++writeValues)
	{
		writingHandler1->setSignedLong(writeValues, writeValues+100);
	}

	// Check the other data handlers. They should still have the original values
		for(imbxInt32 checkValues = 0; checkValues < 10; ++checkValues)
	{
				QCOMPARE(writingHandler2->getSignedLong(checkValues), checkValues);
				QCOMPARE(readingHandler1->getSignedLong(checkValues), checkValues);
				QCOMPARE(readingHandler2->getSignedLong(checkValues), checkValues);
	}

	// Now modify the second writing handler
	writingHandler2->setSize(5);
		QVERIFY(writingHandler2->getSize() == 5);
	for(int writeValues = 0; writeValues < 5; ++writeValues)
	{
		writingHandler2->setSignedLong(writeValues, writeValues+200);
	}

	// Release the first writing handler, then recheck the values in the other handlers
	writingHandler1 = ptr<handlers::dataHandler>(0);
		QVERIFY(readingHandler1->getSize() == 10);
		QVERIFY(readingHandler2->getSize() == 10);
		for(imbxInt32 checkValues = 0; checkValues < 10; ++checkValues)
	{
				QCOMPARE(readingHandler1->getSignedLong(checkValues), checkValues);
				QCOMPARE(readingHandler2->getSignedLong(checkValues), checkValues);
	}
		QVERIFY(writingHandler2->getSize() == 5);
		for(imbxInt32 checkValues = 0; checkValues < 5; ++checkValues)
	{
				QCOMPARE(writingHandler2->getSignedLong(checkValues), checkValues + imbxInt32(200));
	}

	// Get a reading handler. It should have the value written by writingHandler1
	ptr<handlers::dataHandler> readingHandler3 = patientBuffer->getDataHandler(false);
		QVERIFY(readingHandler3->getSize() == 20);
		for(imbxInt32 checkValues = 0; checkValues < 20; ++checkValues)
	{
				QCOMPARE(readingHandler3->getSignedLong(checkValues), checkValues + imbxInt32(100));
	}

	// Release a reading handler. It shouldn't change the values in the buffer
	readingHandler1 = ptr<handlers::dataHandler>(0);
	ptr<handlers::dataHandler> readingHandler4 = patientBuffer->getDataHandler(false);
		QVERIFY(readingHandler4->getSize() == 20);
		for(imbxInt32 checkValues = 0; checkValues < 20; ++checkValues)
	{
				QCOMPARE(readingHandler4->getSignedLong(checkValues), checkValues + imbxInt32(100) );
	}

	// Release the second writing handler. It should change the buffer, but already
	//  existing handlers should continue with their values
	writingHandler2 = ptr<handlers::dataHandler>(0);
	ptr<handlers::dataHandler> writingHandler3 = patientBuffer->getDataHandler(true);
		QVERIFY(writingHandler3->getSize() == 5);
		for(imbxInt32 checkValues = 0; checkValues < 5; ++checkValues)
	{
				QCOMPARE(writingHandler3->getSignedLong(checkValues), checkValues + imbxInt32(200) );
	}
	writingHandler3 = ptr<handlers::dataHandler>(0);

	// ReadingHandler2 still exist. Check its values
		QVERIFY(readingHandler2->getSize() == 10);
		for(imbxInt32 checkValues = 0; checkValues < 10; ++checkValues)
	{
				QCOMPARE(readingHandler2->getSignedLong(checkValues), checkValues);
	}
	readingHandler2 = ptr<handlers::dataHandler>(0);

	// Get a reading handler and check it. It should have the values of writingHandler2
	ptr<handlers::dataHandler> readingHandler5 = patientBuffer->getDataHandler(false);
		QVERIFY(readingHandler5->getSize() == 5);
		for(imbxInt32 checkValues = 0; checkValues < 5; ++checkValues)
	{
				QCOMPARE(readingHandler5->getSignedLong(checkValues), checkValues + imbxInt32(200) );
	}
}

void bufferTest::testOddLength() 
{
	ptr<buffer> patientBuffer(new buffer(ptr<baseObject>(0), "OB"));
	ptr<handlers::dataHandler> writingHandler = patientBuffer->getDataHandler(true);
	
	// Write 3 numbers
	writingHandler->setSize(3);
	writingHandler->setUnsignedLong(0, 10);
	writingHandler->setUnsignedLong(1, 20);
	writingHandler->setUnsignedLong(2, 30);
	writingHandler = ptr<handlers::dataHandler>(0);

	ptr<handlers::dataHandler> readingHandler = patientBuffer->getDataHandler(false);
		QVERIFY(readingHandler->getSize() == 4);
		QVERIFY(readingHandler->getUnsignedLong(0) == 10);
		QVERIFY(readingHandler->getUnsignedLong(1) == 20);
		QVERIFY(readingHandler->getUnsignedLong(2) == 30);
		QVERIFY(readingHandler->getUnsignedLong(3) == 0);
}

} // namespace tests

} // namespace imebra

} // namespace puntoexe
