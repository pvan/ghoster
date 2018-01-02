#ifndef UTILS_H
#define UTILS_H


// todo: move glass and movie depend on this file, try to elim both dependances?

// todo: keep this sep or what?
#include "rand.h"


#define PRINT(...) { char buf[256]; sprintf(buf, __VA_ARGS__); OutputDebugString(buf); }



int nearestInt(double in)
{
    return floor(in + 0.5);
}
i64 nearestI64(double in)
{
    return floor(in + 0.5);
}



float GetWallClockSeconds()
{
    LARGE_INTEGER counter; QueryPerformanceCounter(&counter);
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    return (float)counter.QuadPart / (float)freq.QuadPart;
}



// STRING


// this is case sensitive
bool StringBeginsWith(const char *str, const char *front)
{
    while (*front)
    {
        if (!*str)
            return false;

        if (*front != *str)
            return false;

        front++;
        str++;
    }
    return true;
}
// this is case sensitive
bool StringEndsWith(const char *str, const char *end)
{
    int strLen = strlen(str);
    int endLen = strlen(end);
    for (int i = 0; i < endLen; i++)
    {
        // recall str[len-1] is the last character of the string
        if (str[(strLen-1)-i] != end[(endLen-1)-i]) return false;
    }
    return true;
}
bool Test_StringEndsWith()
{
    assert(StringEndsWith("test.test.txt", ".txt"));
    assert(StringEndsWith("test.txt", ".txt"));
    assert(StringEndsWith("test.txt", "t"));
    assert(StringEndsWith("test.txt\n", "t\n"));
    assert(StringEndsWith("test.txt", ""));
    assert(StringEndsWith("", ""));
    assert(!StringEndsWith("test.txt", ".txt2"));
    assert(!StringEndsWith("test.txt2", ".txt"));
    assert(!StringEndsWith("", "txt"));
    assert(!StringEndsWith("test.txt\n", ".txt"));
    return true;
}

// a simple guess at least
bool StringIsUrl(const char *path)
{
    // feels pretty rudimentary / incomplete
    if (StringBeginsWith(path, "http")) return true;
    if (StringBeginsWith(path, "www")) return true;
    return false;
}

#define NBSP '\u00A0'

void TransmogrifyTextInto(char *dest, char *src)
{
    while (*src)
    {
        *dest = toupper(*src);
        dest++;
        // *dest = ' ';
        *dest = NBSP;  // '\u00A0' nbsp
        dest++;

        src++;
    }
    dest--; // override that last space
    *dest = '\0';
}

void DirectoryFromPath(char *path)
{
    char *new_end = 0;
    for (char *c = path; *c; c++)
    {
        if (*c == '\\' || *c == '/')
            new_end = c+1;
    }
    if (new_end != 0)
        *new_end = '\0';
}



// for timestamps in the form &t=X / &t=Xs / &t=XmYs / &t=XhYmZs
int SecondsFromStringTimestamp(char *timestamp)
{
    int secondsSoFar = 0;

    if (StringBeginsWith(timestamp, "&t=") || StringBeginsWith(timestamp, "#t="))
    {
        timestamp+=3;
    }

    char *p = timestamp;

    char *nextDigits = timestamp;
    int digitCount = 0;

    char *nextUnits;

    while (*p)
    {

        nextDigits = p;
        while (isdigit((int)*p))
        {
            p++;
        }

        int nextNum = atoi(nextDigits);

        // nextUnit = *p;
        // if (nextUnit == '\0') nextUnit = 's';

        int secondsPerUnit = 1;
        if (*p == 'm') secondsPerUnit = 60;
        if (*p == 'h') secondsPerUnit = 60*60;

        secondsSoFar += nextNum*secondsPerUnit;

        p++;

    }

    return secondsSoFar;
}

bool Test_SecondsFromStringTimestamp()
{
    assert(SecondsFromStringTimestamp("#t=21s1m") == 21+60);
    assert(SecondsFromStringTimestamp("&t=216") == 216);
    assert(SecondsFromStringTimestamp("12") == 12);
    assert(SecondsFromStringTimestamp("12s") == 12);
    assert(SecondsFromStringTimestamp("854s") == 854);
    assert(SecondsFromStringTimestamp("2m14s") == 2*60+14);
    assert(SecondsFromStringTimestamp("3h65m0s") == 3*60*60+65*60+0);
    return true;
}



#endif