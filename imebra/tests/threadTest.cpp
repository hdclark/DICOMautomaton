#include "threadTest.h"

#include "../../imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

thread::thread(long setWaitMsec, long endWaitMsec):
        m_setWaitMsec(setWaitMsec), m_endWaitMsec(endWaitMsec), m_bVariable(false)
{
}

void thread::threadFunction()
{
	Sleep(m_setWaitMsec);
	{
		puntoexe::lockCriticalSection lockVariable(&m_lockVariable);
		m_bVariable = true;
	}
	if(shouldTerminate())
	{
		return;
	}
	Sleep(m_endWaitMsec);
}


void threadTest::testThreadsTransactions()
{
	// The test is executed twice, releasing the datasets in
	//  different order
	for(int releaseMode = 0; releaseMode < 2; ++releaseMode)
	{
		m_dataSet0 = ptr<dataSet>(new dataSet);
		m_dataSet1 = ptr<dataSet>(new dataSet);

		m_dataSet0->setUnicodeString(0x10, 0, 0x10, 0, L"test0", "PN");
		m_dataSet1->setUnicodeString(0x10, 0, 0x10, 0, L"test1", "PN");

		m_transactionTestWait = 0;

		// Lock the dataSet 0 and 1, start a thread that requires a transaction
		//  on both the dataSets. Release first one dataSet, then the second one
		DWORD threadId = 0;
		HANDLE hThread = CreateThread(NULL, 0, threadTest::testTransaction, this, 0, &threadId);

		ptr<dataSet> release0;
		ptr<dataSet> release1;
		if(releaseMode == 0)
		{
			release0 = m_dataSet0;
			release1 = m_dataSet1;
		}
		else
		{
			release0 = m_dataSet1;
			release1 = m_dataSet0;
		}

		Sleep(1000);
		{
			lockObject lockAccess(release0.get());
			{
				lockObject lockAccess(release1.get());
				Sleep(3000);
			}
			m_dataSet0->setUnicodeString(0x10, 0, 0x10, 0, L"mytest0", "PN");
			m_dataSet1->setUnicodeString(0x10, 0, 0x10, 0, L"mytest1", "PN");
			CPPUNIT_ASSERT(m_dataSet0->getUnicodeString(0x10, 0, 0x10, 0) == L"mytest0");
			CPPUNIT_ASSERT(m_dataSet1->getUnicodeString(0x10, 0, 0x10, 0) == L"mytest1");
			Sleep(3000);
		}

		DWORD threadWait = WaitForSingleObject(hThread, 20000);
		CPPUNIT_ASSERT(threadWait == WAIT_OBJECT_0);
		CPPUNIT_ASSERT(m_transactionTestWait >= 6500 && m_transactionTestWait < 10000);

		CPPUNIT_ASSERT(m_dataSet0->getUnicodeString(0x10, 0, 0x10, 0) == L"threadTest0");
		CPPUNIT_ASSERT(m_dataSet1->getUnicodeString(0x10, 0, 0x10, 0) == L"threadTest1");
	}
}

DWORD WINAPI threadTest::testTransaction(LPVOID lpParam)
{
	// Timer
	DWORD startTime = GetTickCount();
	threadTest* pClass = (threadTest*)lpParam;

	IMEBRA_TRANSACTION_START();

	pClass->m_dataSet0->setUnicodeString(0x10, 0, 0x10, 0, L"threadTest0", "PN");
	pClass->m_dataSet1->setUnicodeString(0x10, 0, 0x10, 0, L"threadTest1", "PN");

	Sleep(2000);

	IMEBRA_TRANSACTION_END();
	
	pClass->m_transactionTestWait = GetTickCount() - startTime;

	return 0;
}


void threadTest::testThreads()
{
	ptr<thread> test(new thread(300, 0));
	test->start();
	DWORD startTime = GetTickCount();
	test.release();
	DWORD endTime = GetTickCount();
	CPPUNIT_ASSERT(endTime - startTime >= 250 && endTime - startTime <= 380);

	test = ptr<thread>(new thread(300, 200));
	CPPUNIT_ASSERT(!test->isRunning());
	test->start();
	startTime = GetTickCount();
	Sleep(350);
	CPPUNIT_ASSERT(test->isRunning());
	test.release();
	endTime = GetTickCount();
	CPPUNIT_ASSERT(endTime - startTime >= 490 && endTime - startTime <= 580);

	test = ptr<thread>(new thread(300, 200));
	CPPUNIT_ASSERT(!test->isRunning());
	test->start();
	startTime = GetTickCount();
	Sleep(100);
	CPPUNIT_ASSERT(test->isRunning());
	test.release();
	endTime = GetTickCount();
	CPPUNIT_ASSERT(endTime - startTime >= 290 && endTime - startTime <= 380);

	test = ptr<thread>(new thread(300, 200));
	test->start();
	test->terminate();
	startTime = GetTickCount();
	::Sleep(50);
	CPPUNIT_ASSERT(test->isRunning());
	::Sleep(300);
	CPPUNIT_ASSERT(!test->isRunning());
	test.release();
	endTime = GetTickCount();
	CPPUNIT_ASSERT(endTime - startTime >= 340 && endTime - startTime <= 480);
}


void threadTest::testThreadsExceptions()
{
	DWORD threadId = 0;
	HANDLE hThread = CreateThread(NULL, 0, threadTest::testException, this, 0, &threadId);

	std::wstring mainMessage;
	for(int throwExc = 0; throwExc < 1000; ++throwExc)
	{
		try
		{
			PUNTOEXE_FUNCTION_START(L"mainThread");
			{
				PUNTOEXE_FUNCTION_START(L"mainThread0");
				PUNTOEXE_THROW(std::runtime_error, "main thread exc");
				PUNTOEXE_FUNCTION_END();
			}
			PUNTOEXE_FUNCTION_END();
		}
		catch(std::runtime_error&)
		{
			mainMessage += exceptionsManager::getMessage();
		}
	}
	
	WaitForSingleObject(hThread, INFINITE);
	
	size_t offsetMainThread = 0;
	size_t offsetMainThread0 = 0;
	size_t offsetMainThreadExc = 0;
	size_t offsetSecThread = 0;
	size_t offsetSecThread0 = 0;
	size_t offsetSecThreadExc = 0;
	for(int check = 0; check < 1000; ++check)
	{
		size_t findMainThread = mainMessage.find(L"mainThread", offsetMainThread);
		offsetMainThread = findMainThread + 1;
		CPPUNIT_ASSERT(findMainThread != std::string::npos);

		size_t findMainThread0 = mainMessage.find(L"mainThread0", offsetMainThread0);
		offsetMainThread0 = findMainThread0 + 1;
		CPPUNIT_ASSERT(findMainThread0 != std::string::npos);

		size_t findMainThreadExc = mainMessage.find(L"main thread exc", offsetMainThreadExc);
		offsetMainThreadExc = findMainThreadExc + 1;
		CPPUNIT_ASSERT(findMainThreadExc != std::string::npos);

		size_t findSecThread = m_threadMessage.find(L"secThread", offsetSecThread);
		offsetSecThread = findSecThread + 1;
		CPPUNIT_ASSERT(findSecThread != std::string::npos);

		size_t findSecThread0 = m_threadMessage.find(L"secThread0", offsetSecThread0);
		offsetSecThread0 = findSecThread0 + 1;
		CPPUNIT_ASSERT(findSecThread0 != std::string::npos);

		size_t findSecThreadExc = m_threadMessage.find(L"secondary thread exc", offsetSecThreadExc);
		offsetSecThreadExc = findSecThreadExc + 1;
		CPPUNIT_ASSERT(findSecThreadExc != std::string::npos);
	}

	CPPUNIT_ASSERT(mainMessage.find(L"secThread") == std::string::npos);
	CPPUNIT_ASSERT(mainMessage.find(L"secThread0") == std::string::npos);
	CPPUNIT_ASSERT(mainMessage.find(L"secondary thread exc") == std::string::npos);

	CPPUNIT_ASSERT(m_threadMessage.find(L"mainThread") == std::string::npos);
	CPPUNIT_ASSERT(m_threadMessage.find(L"mainThread0") == std::string::npos);
	CPPUNIT_ASSERT(m_threadMessage.find(L"main thread exc") == std::string::npos);
}

DWORD WINAPI threadTest::testException(LPVOID lpParam)
{
	std::wstring message;
	for(int throwExc = 0; throwExc < 1000; ++throwExc)
	{
		try
		{
			PUNTOEXE_FUNCTION_START(L"secThread");
			{
				PUNTOEXE_FUNCTION_START(L"secThread0");
				PUNTOEXE_THROW(std::runtime_error, "secondary thread exc");
				PUNTOEXE_FUNCTION_END();
			}
			PUNTOEXE_FUNCTION_END();
		}
		catch(std::runtime_error&)
		{
			message += exceptionsManager::getMessage();
		}
	}

	((threadTest*)lpParam)->m_threadMessage = message;
	
	return 0;
}

} // namespace tests

} // namespace imebra

} // namespace puntoexe
