//Process_Fork.h.

#pragma once


class ProcessForkGuard {
    public:
        ProcessForkGuard();
        ~ProcessForkGuard();

        void release();

        ProcessForkGuard(const ProcessForkGuard &) = delete;
        ProcessForkGuard & operator=(const ProcessForkGuard &) = delete;

    private:
        bool reserved_ = false;
};

void DisableForkForPythonInitialization();
