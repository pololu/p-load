#pragma once

#include "p-load.h"
#include "ploader.h"

/* This object manages the standard output of the process. */
class Output : public PloaderStatusListener
{
    static const uint32_t barWidth = 58;

public:
    Output();
    void startNewLine();
    bool shouldPrintInfo();
    void setStatus(const char * status, uint32_t progress, uint32_t maxProgress);

private:
    bool printInfoFlag;
    bool currentLineHasBar;
    uint32_t currentBarLength;
    std::string currentMessage;
};

