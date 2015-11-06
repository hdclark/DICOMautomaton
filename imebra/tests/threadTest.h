#if !defined(imebraThreadTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraThreadTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include "windows.h"
#include "../../imebra/include/imebra.h"

#include <QtTest/QtTest>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

class thread: public puntoexe::thread
{
public:
	thread(long setWaitMsec, long endWaitMsec);
	virtual void threadFunction();

	long m_setWaitMsec;
	long m_endWaitMsec;

	puntoexe::criticalSection m_lockVariable;
	bool m_bVariable;
};


class threadTest: public QObject
{
	Q_OBJECT

private slots:
	void testThreadsTransactions();
	void testThreads();

private:
	static DWORD WINAPI testTransaction(LPVOID lpParam);
	DWORD m_transactionTestWait;
	ptr<dataSet> m_dataSet0;
	ptr<dataSet> m_dataSet1;

	void testThreadsExceptions();
	static DWORD WINAPI testException(LPVOID lpParam);
	std::wstring m_threadMessage;


};

} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraThreadTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
