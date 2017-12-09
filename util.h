



void MsgBox(char *s) {
    MessageBox(0, s, "vid player", MB_OK);
}





int randomInt(int upToAndNotIncluding)
{
    // todo: oddly this only-seed-once code is throwing something off
    // todo: replace with proper randomness anyway (lib?)

    // static bool global_already_seeded_rand = false;
    // if (!global_already_seeded_rand)
    // {
        // global_already_seeded_rand = true;
        srand(GetTickCount());
    // }
    return rand() % upToAndNotIncluding;
}



int nearestInt(double in)
{
    return floor(in + 0.5);
}
i64 nearestI64(double in)
{
    return floor(in + 0.5);
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


void TransmogrifyText(char *src, char *dest)
{
    while (*src)
    {
        *dest = toupper(*src);
        dest++;
        // *dest = ' ';
        *dest = '\u00A0';  // nbsp
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
