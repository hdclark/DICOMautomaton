//Thread_Pool.h.

#pragma once

// This header previously contained a local copy of the work_queue thread pool class.
// That implementation has been superseded by the Ygor library's equivalent.
// It also historically pulled in several standard-library headers transitively for callers.

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <list>
// Include the Ygor header directly to get the work_queue class.
#include "YgorThreadPool.h"
