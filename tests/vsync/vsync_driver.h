#pragma once

#include <cstdint>
#include <string>

namespace VSyncTest
{
enum class Support
{
    Primary, Secondary, Legacy, All, Unavailable, SimilarName
};

struct Driver
{
    Support support = Support::Primary;
    bool missingFunction = false;
    std::string missingEntry;
    bool fail = false;
    bool arbString = true;
    bool extString = true;
    bool nullStrings = false;
    bool direct = true;
    std::intptr_t invalidAddress = 0;
    int interval = -1;
    int calls = 0;
    std::string selected;
};

extern Driver driver;
void reset();
void require(bool condition, const char* message);
void runPlatformCases();
}
