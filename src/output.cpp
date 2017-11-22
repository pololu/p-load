#include "output.h"

Output::Output()
{
    // TODO: get this to work in MSYS2 shells
    printInfoFlag = isatty(fileno(stdout));
    currentLineHasBar = false;
}

void Output::startNewLine()
{
    if (currentLineHasBar)
    {
        std::cout << std::endl;
    }
    currentLineHasBar = false;
    currentMessage = "";
}

bool Output::shouldPrintInfo()
{
    return printInfoFlag;
}

void Output::setStatus(const char * status, uint32_t progress, uint32_t maxProgress)
{
    if (!shouldPrintInfo()) { return; }

    if (currentMessage != status)
    {
        startNewLine();
        currentMessage = status;
        std::cout << currentMessage << std::endl;
    }

    if (maxProgress)
    {
        uint32_t scaledProgress = progress * barWidth / maxProgress;
        if (!currentLineHasBar || currentBarLength != scaledProgress)
        {
            uint32_t i;
            std::cout << "\rProgress: |";
            for (i = 0; i < scaledProgress; i++){ std::cout << '#'; }
            for (; i < barWidth; i++){ std::cout << ' '; }
            std::cout << '|';
            currentLineHasBar = true;
            currentBarLength = scaledProgress;
            if (progress == maxProgress)
            {
                std::cout << " Done.";
                startNewLine();
            }
            std::cout << std::flush;
        }
    }
}

void Output::printInfo(const char * message)
{
  if (!shouldPrintInfo()) { return; }
  startNewLine();
  std::cout << message << std::endl;
}
