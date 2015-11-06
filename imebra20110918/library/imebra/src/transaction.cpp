/*

Imebra 2011 build 2011-09-18_22-24-41

Imebra: a C++ Dicom library

Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 by Paolo Brandoli
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors
 and should not be interpreted as representing official policies, either expressed or implied,
 of the Imebra Project.

Imebra is available at http://imebra.com



*/

/*! \file transaction.cpp
    \brief Implementation of the class transaction.

*/

#include "../include/transaction.h"

namespace puntoexe
{

namespace imebra
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// transaction
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Constructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
transaction::transaction(bool bCommitTransaction)
{
	// Save the thread id
	///////////////////////////////////////////////////////////
	m_threadId = puntoexe::thread::getThreadId();

	// Set the commit flag. The flag is always true if there
	//  are no parent transactions
	///////////////////////////////////////////////////////////
	m_bCommit = transactionsManager::addTransaction(m_threadId, this) || bCommitTransaction;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Destructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
transaction::~transaction()
{
	PUNTOEXE_FUNCTION_START(L"transaction::~transaction");

	if(m_bCommit)
	{
		// We have to commit the changes
		///////////////////////////////////////////////////////////
		try
		{
			// Lock all the buffers involved in the commit
			///////////////////////////////////////////////////////////
			lockMultipleObjects::tObjectsList objectsList;
			for(tHandlersList::iterator findBuffers = m_transactionHandlers.begin(); findBuffers != m_transactionHandlers.end(); ++findBuffers)
			{
				objectsList.push_back(findBuffers->second->m_buffer);
			}
			lockMultipleObjects lockAccess(&objectsList);

			// Copy the data back to the buffers
			///////////////////////////////////////////////////////////
			for(tHandlersList::iterator scanHandlers = m_transactionHandlers.begin(); scanHandlers != m_transactionHandlers.end(); ++scanHandlers)
			{
				scanHandlers->second->copyBack();
			}

			// Commit the changes
			///////////////////////////////////////////////////////////
			for(tHandlersList::iterator scanCommits = m_transactionHandlers.begin(); scanCommits != m_transactionHandlers.end(); ++scanCommits)
			{
				scanCommits->second->commit();
			}

			// Remove all the data handlers
			///////////////////////////////////////////////////////////
			m_transactionHandlers.clear();
		}
		catch(...)
		{
			// The commit failed.
			// Abort the modifications and rethrow the exception
			///////////////////////////////////////////////////////////
			abort();
			transactionsManager::removeTransaction(m_threadId);
			PUNTOEXE_RETHROW("Commit failed");
		}
	}

	transactionsManager::removeTransaction(m_threadId);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Add a dataHandler to the transaction
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void transaction::addHandler(ptr<handlers::dataHandler> newHandler)
{
	PUNTOEXE_FUNCTION_START(L"transaction::addHandler");

        tHandlersList::iterator findPreviousHandler(m_transactionHandlers.find(newHandler->m_buffer.get()));
        if(findPreviousHandler != m_transactionHandlers.end())
        {
            findPreviousHandler->second->abort();
        }
        m_transactionHandlers[newHandler->m_buffer.get()] = newHandler;
	
	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Abort the transaction
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void transaction::abort()
{
	PUNTOEXE_FUNCTION_START(L"transaction::abort");

	for(tHandlersList::iterator scanHandlers = m_transactionHandlers.begin(); scanHandlers != m_transactionHandlers.end(); ++scanHandlers)
	{
		scanHandlers->second->abort();
	}
	m_transactionHandlers.clear();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Copy the dataHandlers to another transaction
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void transaction::copyHandlersTo(transaction* pDestination)
{
	for(tHandlersList::iterator scanHandlers = m_transactionHandlers.begin(); scanHandlers != m_transactionHandlers.end(); ++scanHandlers)
	{
            tHandlersList::iterator findPreviousHandler(pDestination->m_transactionHandlers.find(scanHandlers->second->m_buffer.get()));
            if(findPreviousHandler != pDestination->m_transactionHandlers.end())
            {
                findPreviousHandler->second->abort();
            }
            pDestination->m_transactionHandlers[scanHandlers->second->m_buffer.get()] = scanHandlers->second;
	}
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// transactionManager
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Add a transaction to the current thread's transactions
//  stack
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool transactionsManager::addTransaction(thread::tThreadId threadId, transaction* pTransaction)
{
	PUNTOEXE_FUNCTION_START(L"transactionsManager::addTransaction");

	// Retrieve the transactions manager and lock it
	///////////////////////////////////////////////////////////
	transactionsManager* pManager = getTransactionsManager();
	lockObject(pManager->m_lockObject.get());

	// Push back the transaction and return true if this is
	//  the first transaction in the stack
	///////////////////////////////////////////////////////////
	pManager->m_transactions[threadId].push_back(pTransaction);
	return pManager->m_transactions[threadId].size() == 1;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Remove a transaction from the transactions stack
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void transactionsManager::removeTransaction(thread::tThreadId threadId)
{
	PUNTOEXE_FUNCTION_START(L"transactionsManager::removeTransaction");

	// Retrieve the transactions manager and lock it
	///////////////////////////////////////////////////////////
	transactionsManager* pManager = getTransactionsManager();
	lockObject(pManager->m_lockObject.get());

	// Find the thread's transactions stack
	///////////////////////////////////////////////////////////
	tTransactionsMap::iterator findThread = pManager->m_transactions.find(threadId);
	if(findThread == pManager->m_transactions.end())
	{
		PUNTOEXE_THROW(std::logic_error, "Transaction not found in the transactions stack");
	}

	// Pop the last transaction in the stack
	///////////////////////////////////////////////////////////
	transaction* pLastTransaction = findThread->second.back(); // This throw if the stack is empty. It's OK
	findThread->second.pop_back();

	// If there are no parent transactions then return
	///////////////////////////////////////////////////////////
	if(findThread->second.empty())
	{
		pManager->m_transactions.erase(findThread);
		return;
	}

	// If there is a parent transaction then copy there the 
	//  dataHandlers from the removed transaction
	///////////////////////////////////////////////////////////
	transaction* pParentTransaction = findThread->second.back();
	pLastTransaction->copyHandlersTo(pParentTransaction);

	PUNTOEXE_FUNCTION_END();
}


void transactionsManager::addHandlerToTransaction(ptr<handlers::dataHandler> newHandler)
{
	PUNTOEXE_FUNCTION_START(L"transactionsManager::addHandlerToTransaction");

	// Retrieve the transactions manager and lock it
	///////////////////////////////////////////////////////////
	transactionsManager* pManager = getTransactionsManager();
	lockObject(pManager->m_lockObject.get());

	// Find the thread's transactions stack
	///////////////////////////////////////////////////////////
	thread::tThreadId threadId = puntoexe::thread::getThreadId();
	tTransactionsMap::iterator findThread = pManager->m_transactions.find(threadId);
	if(findThread == pManager->m_transactions.end())
	{
		return;
	}

	// Get the last transaction in the stack
	///////////////////////////////////////////////////////////
	transaction* pLastTransaction = findThread->second.back(); // This throw if the stack is empty. It's OK
	pLastTransaction->addHandler(newHandler);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return a pointer to the transactions manager
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
transactionsManager* transactionsManager::getTransactionsManager()
{
	static transactionsManager m_manager;
	return &m_manager;
}

} // namespace imebra

} // namespace puntoexe
