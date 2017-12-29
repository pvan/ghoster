

const bool DEBUG_URL_MSG = true;

char *TEST_FILES[] = {
    "D:/~phil/projects/ghoster/test-vids/tall.mp4",
    "D:/~phil/projects/ghoster/test-vids/testcounter30fps.webm",
    // "D:/~phil/projects/ghoster/test-vids/sync3.mp4",
    "D:/~phil/projects/ghoster/test-vids/sync1.mp4",
    "D:/~phil/projects/ghoster/test-vids/test4.mp4",
    "D:/~phil/projects/ghoster/test-vids/test.mp4",
    "D:/~phil/projects/ghoster/test-vids/test3.avi",
    "D:/~phil/projects/ghoster/test-vids/test.3gp",
    "https://www.youtube.com/watch?v=RYbe-35_BaA",
    "https://www.youtube.com/watch?v=ucZl6vQ_8Uo",
    "https://www.youtube.com/watch?v=tprMEs-zfQA",
    "https://www.youtube.com/watch?v=NAvOdjRsOoI"
};

char *RANDOM_VIDEOS[] = {
    "https://www.youtube.com/watch?v=RYbe-35_BaA",  // 7-11
    "https://www.youtube.com/watch?v=ucZl6vQ_8Uo",  // AV sync
    "https://www.youtube.com/watch?v=tprMEs-zfQA",  // mother of all funk chords
    // "https://www.youtube.com/watch?v=K0uxjdcZWyk",  // AV sync with LR  // todo: fails??
    "https://www.youtube.com/watch?v=FI-4HNQg1JI",  // oscar peterson
    "https://www.youtube.com/watch?v=OJpQgSZ49tk",  // that one vid from prototype dev
    "https://www.youtube.com/watch?v=dzUNFqOwjfA",  // timelapse
    "https://www.youtube.com/watch?v=6D-A6CL3Pv8",  // timelapse
    "https://www.youtube.com/watch?v=Pi8k1lTqrkQ",  // music video
    "https://www.youtube.com/watch?v=gRAw5wsAKik",  // music video
    "https://www.youtube.com/watch?v=rpWUDU_GEL4",  // little sadie
    "https://www.youtube.com/watch?v=NVb5GV6lntU",  // psychill mix
    "https://www.youtube.com/watch?v=8t3XYNxnUBs",  // psychill mix
    "https://www.youtube.com/watch?v=3WLNhLs5sFg",  // ambient sunrise
    "https://www.youtube.com/watch?v=P5_GlAOCHyE",  // space
    "https://www.youtube.com/watch?v=DgPaCWJL7XI",  // deep dream grocery
    "https://www.youtube.com/watch?v=FFoPYw55C_c",  // pixel music vid
    "https://www.youtube.com/watch?v=iYUh88gr7DI",  // popeye tangerine dream
    "https://www.youtube.com/watch?v=9wPb07EPDD4",  // "visual project"
    "https://www.youtube.com/watch?v=XxfNqvoXRug",  // porches
    "https://www.youtube.com/watch?v=wA2APi0cTYY",  // satantango
    "https://www.youtube.com/watch?v=F0O_6nMvqiM",  // yule log
    "https://www.youtube.com/watch?v=aia3bqQfXNk",  // l'eclisse
    "https://www.youtube.com/watch?v=UW8tpjJt0xc",  // rabbit hole 2 mix
    "https://www.youtube.com/watch?v=APmBT96ETJk",  // kiki mix
    "https://www.youtube.com/watch?v=p_FF-VN7xmg",  // snow day in raleigh
    "https://www.youtube.com/watch?v=XKDGZ-VWLMg",  // raining in tokyo
    "https://www.youtube.com/watch?v=_JPa3BNi6l4",  // gondry daft p backwards
    "https://www.youtube.com/watch?v=kGN0B0WqCgM",  // volare
    "https://www.youtube.com/watch?v=gZo1BLYcMJ4",  // mario rpg
    "https://www.youtube.com/watch?v=i53jrrMT6XQ",  // singsingsing reversed
    "https://www.youtube.com/watch?v=lJJW0dE5GF0",  // queen of the night
    "https://www.youtube.com/watch?v=EyPyjprGvW0",  // dragon roost
    "https://www.youtube.com/watch?v=hqXpaTu8UrM",  // gymnopedie take five
    "https://www.youtube.com/watch?v=qpMIijaTePA",  // samantha fish
    "https://www.youtube.com/watch?v=CN3-4Z4ae_0",  // wind games
    "https://www.youtube.com/watch?v=aeEQDtk63H4",  // daft train
    "https://www.youtube.com/watch?v=pC0HpEq-rb0",  // music video
    "https://www.youtube.com/watch?v=aB4M9rl8GvM",  // big charade
    "https://www.youtube.com/watch?v=uieM18rZdHY",  // fox in space
    "https://www.youtube.com/watch?v=NAh9oLs67Cw&t=60",  // garfield
    "https://www.youtube.com/watch?v=a9zvWR14KJQ",  // synthwave mix
    "https://www.youtube.com/watch?v=6gYBXRwsDjY",  // transfiguration
    "https://www.youtube.com/watch?v=L8CxZWazgxY",  // cousins?
    "https://www.youtube.com/watch?v=T6JEW93Ock8",  // somewhere in cali
    "https://www.youtube.com/watch?v=u1MKUJN7vUk",  // bande a part
    "https://www.youtube.com/watch?v=69gZRgMcNZo",  // down by law
    "https://www.youtube.com/watch?v=OQN6Gkv4JRU",  // fallen angels
    "https://www.youtube.com/watch?v=zdjXoZB-Oc4",  // days of being wild
    "https://www.youtube.com/watch?v=M1F0lBnsnkE",  // uptown funk hollywood dance supercut
    "https://www.youtube.com/watch?v=ahoJReiCaPk",  // Hellzapoppin
    "https://www.youtube.com/watch?v=PVG_QA5stBc",  // dancing at the movies
    "https://www.youtube.com/watch?v=E-6xk4W6N20",  // disney to madeon
    "https://www.youtube.com/watch?v=nUWur-T598s",  // brick chase
    "https://www.youtube.com/watch?v=2jQ-ehu3ZS8",  // strictly ballroom
    "https://www.youtube.com/watch?v=UUO5WPaIr-s",  // strictly ballroom paso doble
    "https://www.youtube.com/watch?v=hOZKbOwNGhQ",  // ratcatcher window scene
    "https://www.youtube.com/watch?v=GTFaOxLlJCA",  // blue cheer
    "https://www.youtube.com/watch?v=7Z0lNch5qkQ",  // morvern callar scene
    "https://www.youtube.com/watch?v=11p0y9z1XOU",  // quinoa w/ lynch
    "https://www.youtube.com/watch?v=TmoBMjbY5Nw",  // pierrot le fou car
    "https://www.youtube.com/watch?v=LmWaoovzYlw",  // pierrot le fou beach
    // "https://www.youtube.com/watch?v=HW8f6V0beH8",  // buona sera  // todo: fail?
    "https://www.youtube.com/watch?v=dQEmaj9C6ko",  // BoC video
    "https://www.youtube.com/watch?v=0o9qDBFKmGw",  // my brightest diamond
    "https://www.youtube.com/watch?v=19r7ctge2lI&t=18",  // birds
    "https://www.youtube.com/watch?v=hi4pzKvuEQM",  // chet faker
    "https://www.youtube.com/watch?v=kMyIFrEZtnw",  // raw run
    "https://www.youtube.com/watch?v=LscksnGO09Q",  // raw run puebla
    "https://www.youtube.com/watch?v=RjzC1Dgh17A",  // ghostbusters
    "https://www.youtube.com/watch?v=6KES5UH6fHE",  // peking opera blues
    "https://www.youtube.com/watch?v=ygI-2F8ApUM",  // brodyquest
    "https://www.youtube.com/watch?v=ekdLYH04LHQ",  // electric feel cover
    "https://www.youtube.com/watch?v=ckJVotYWyRQ",  // portico quartet live
    "https://www.youtube.com/watch?v=GRPw0xGwYNQ",  // smore jazz, tv
    "https://www.youtube.com/watch?v=Pv-Do30-P8A",  // graffiti timelapse
    "https://www.youtube.com/watch?v=hE7l6Adoiiw",  // coming from biomed?
    "https://www.youtube.com/watch?v=Sk_ga0Y_lnM",  // russian dance
    "https://www.youtube.com/watch?v=voBp8BLU9Gk",  // magenta
    "https://www.youtube.com/watch?v=HFBjfzsOtx0",  // spaceship ambience
    "https://www.youtube.com/watch?v=9pVWfzsgLoQ",  // train nordland line
    "https://www.youtube.com/watch?v=u9a1EQS_9Wo",  // ambient space, alien  // todo: cut?
    "https://www.youtube.com/watch?v=m4oZZhpMXP4",  // cathedral mix
    "https://www.youtube.com/watch?v=6ddO3jPUFpg",  // winter ambient
    "https://www.youtube.com/watch?v=g9fZ9YZsQ9A",  // uakti live
    "https://www.youtube.com/watch?v=T9hHKYfXIE0",  // monkees shred
    "https://www.youtube.com/watch?v=w4x-571JN3Q",  // fake dr levin
    "https://www.youtube.com/watch?v=Gel58oDnB3c&t=690",  // manups dance party
    "https://www.youtube.com/watch?v=MLrC7e3vSv8",  // africa toto cover
    "https://www.youtube.com/watch?v=IBd26L_MWHQ",  // fractal 3d
    "https://www.youtube.com/watch?v=M21g2rVwlV8",  // ambient guitar live
    "https://www.youtube.com/watch?v=LI0YmPY1g2E",  // more ambient
    "https://www.youtube.com/watch?v=kQva3_lBNaY",  // tintinology
    "https://www.youtube.com/watch?v=lt8rfsm2mUE",  // possibly in michigan
    "https://www.youtube.com/watch?v=KH4NrUxcsYs",  // how to crush can
    "https://www.youtube.com/watch?v=SqFu5O-oPmU",  // video games and the human condition
    "https://www.youtube.com/watch?v=7uUl_aTbOzQ",  // jon blow on deep work // todo: full
    "https://www.youtube.com/watch?v=YwSZvHqf9qM",  // tangled up in blue live
    "https://www.youtube.com/watch?v=8pTEmbeENF4",  // bret victor
    "https://www.youtube.com/watch?v=JQRRnAhmB58",  // dancing in the rain
    "https://www.youtube.com/watch?v=90TzDXjWTdo",  // daft charleston
    "https://www.youtube.com/watch?v=SAMEIH2_f1k",  // squash & stretch
    "https://www.youtube.com/watch?v=hsNKSbTNd5I",  // dakhabrakha tiny desk
    "https://www.youtube.com/watch?v=o7rpXLdgtEY",  // beirut
    "https://www.youtube.com/watch?v=CBrj4S24074",  // dragon speech
    "https://www.youtube.com/watch?v=cDVBtuWkMS8",  // horowitz plays chopin
    "https://www.youtube.com/watch?v=TNSbRj8sKAs",  // soronprfbs door
    "https://www.youtube.com/watch?v=5-3r3UAMz48",  // street knights
    "https://www.youtube.com/watch?v=xuXignAhNpw",  // 1965 chess AP
    "https://www.youtube.com/watch?v=_5Hr1C62Smk",  // koyaanisqatsi backwards
    "https://www.youtube.com/watch?v=00jWiadrkTo",  // rocket league
    "https://www.youtube.com/watch?v=9ZX_XCYokQo",  // glenn gould
    "https://www.youtube.com/watch?v=04abkYHAbO4",  // art style test
    "https://www.youtube.com/watch?v=UhHYQTK5RWo",  // libertango live
    "https://www.youtube.com/watch?v=AXTwB-NftjA",  // kid at wedding
    "https://www.youtube.com/watch?v=4KzFe50RQkQ",  // wind trees ambient
    "https://www.youtube.com/watch?v=InbaU387Wl8",  // pepe silvia w/ drums
    "https://www.youtube.com/watch?v=KgqORthqbuM?t=114",  // raise the dead live
    "https://www.youtube.com/watch?v=HJD-GeSJ-oY",  // mathnet trial
    "https://www.youtube.com/watch?v=WJiCUdLBxuI",  // van session 17
    "https://www.youtube.com/watch?v=63gdelpCp4k",  // love like a sunset

    // non-youtube tests...

    "http://www.gdcvault.com/play/1014652/An-Apology-for-Roger",
    "https://www.dailymotion.com/video/x8pzuj" // jil is lucky
    // "http://www.dailymotion.com/video/xxhhuh",  // peking opera blues pt1 (sloooow dl)

    // // ffmpeg errors on these
    // "https://vimeo.com/137025856",  // barcelona
    // "https://vimeo.com/242445221",  // tarkovsky candle (we'll blame the witness for getting the yt version blocked)
    // "https://vimeo.com/241007433",  // art of flying
    // "https://player.vimeo.com/video/106771962", // soderbergh raiders

    // works at time or writing but soundcloud seems to break youtube-dl frequently
    // todo: either update youtube-dl automatically or don't include this
    // "https://soundcloud.com/otherpeoplerecords/csp06-nicolas-jaar-essential",
};
const int VIDS_COUNT = sizeof(RANDOM_VIDEOS) / sizeof(RANDOM_VIDEOS[0]);

char *XMAS_SPECIAL[] = {
    "https://www.youtube.com/watch?v=-ZggJNsAuIw",  // sleight ride in 7/8
    "https://www.youtube.com/watch?v=YvI_FNrczzQ",  // vince guaraldi
    "https://www.youtube.com/watch?v=dNUbEDPWrvw",  // sufjan i'll be home for xmas
};
const int XMAS_COUNT = sizeof(XMAS_SPECIAL) / sizeof(XMAS_SPECIAL[0]);

char *MAY_SPECIAL[] = {
    "https://www.youtube.com/watch?v=HAtWd_7ZpF8"  // sufjan piano
};
const int MAY_COUNT = sizeof(MAY_SPECIAL) / sizeof(MAY_SPECIAL[0]);

static int *alreadyPlayedRandos = 0;
static int alreadyPlayedCount = 0;
bool alreadyPlayed(int index)
{
    if (!alreadyPlayedRandos) return false;
    for (int i = 0; i < alreadyPlayedCount; i++)
    {
        if (alreadyPlayedRandos[i] == index)
            return true;
    }
    return false;
}
// void outputAlreadyPlayedList()
// {
//     if (!alreadyPlayedRandos) return;
//     for (int i = 0; i < alreadyPlayedCount; i++)
//     {
//         char buf[123];
//         sprintf(buf, "\n%i\n", alreadyPlayedRandos[i]);
//         OutputDebugString(buf);
//     }
// }

// get_unplayed_index(char **lists, int *listLengths, int listLength)
// {
//     for (int i = 0; i < listLength; i++)
//     {

//     }

// }

const int MAX_LISTS_SUPPORTED = 10;  // increase if we have more than this many lists
struct super_list
{
    char **lists[MAX_LISTS_SUPPORTED];
    int listCounts[MAX_LISTS_SUPPORTED];
    int listCount = 0; // should be less than MAX_LISTS_SUPPORTED

    void add(char **list, int length)
    {
        assert(listCount < MAX_LISTS_SUPPORTED);
        lists[listCount] = list;
        listCounts[listCount] = length;
        listCount++;
    }
    char *get(int index)
    {
        // better would be to check which list it's in, convert to that index, then return from that list
        int test_index = 0;
        for (int i = 0; i < listCount; i++)
        {
            for (int j = 0; j < listCounts[i]; j++)
            {
                if (test_index == index) return lists[i][j];
                test_index++;
            }
        }
        return 0;
    }
    int total_count()
    {
        int total = 0;
        for (int i = 0; i < listCount; i++)
        {
            total += listCounts[i];
        }
        return total;
    }
};

super_list allvids = {0};
char *get_random_url()
{
    if (!alreadyPlayedRandos)
    {
        // only build the list once per execution so lengths dont change

        allvids.add(RANDOM_VIDEOS, VIDS_COUNT);

        char month[32];
        char day[32];
        GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, 0, "M", (char*)&month, 32);
        GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, 0, "d", (char*)&day, 32);

        if (DEBUG_URL_MSG) PRINT("date: %s-%s\n", month, day);

        if (strcmp(month, "12")==0)
        {
            if (DEBUG_URL_MSG) OutputDebugString("\nDEC\n\n");
            allvids.add(XMAS_SPECIAL, XMAS_COUNT);
        }

        if (strcmp(month, "5")==0 && strcmp(day, "15")==0)
        {
            if (DEBUG_URL_MSG) OutputDebugString("\nMAY\n\n");
            allvids = {0};
            allvids.add(MAY_SPECIAL, MAY_COUNT);
        }

        // note alloc after building lists so we have enough space
        alreadyPlayedRandos = (int*)malloc(sizeof(int) * allvids.total_count());


        if (DEBUG_URL_MSG)
        {
            PRINT("list count: %i\n", allvids.listCount);
            PRINT("url count: %i\n", allvids.total_count());
            for (int i = 0; i < allvids.total_count(); i++)
            {
                PRINT("%i: %s\n", i, allvids.get(i));
            }
        }
    }

    int randomCount = allvids.total_count();

    if (alreadyPlayedCount >= randomCount) // we've gone through every video once
    {
        if (randomCount > 1) // only save last if more than one
        {
            if (DEBUG_URL_MSG) PRINT("\nWENT THRU ALL URLS, saving %i\n\n", alreadyPlayedRandos[alreadyPlayedCount-1]);
            // keep our last video so we never get same twice in row
            alreadyPlayedRandos[0] = alreadyPlayedRandos[alreadyPlayedCount-1];
            alreadyPlayedCount = 1;
        }
    }

    int r = randomInt(randomCount);
    int count = 0;
    while (alreadyPlayed(r) && count++<10000)
    {
        r = randomInt(randomCount);
    }
    if (DEBUG_URL_MSG)
    {
        PRINT("\nusing index %i\n",r);
        OutputDebugString("already used: ");
        for (int i = 0; i < alreadyPlayedCount; i++)
        {
            char buf[123];
            sprintf(buf, "%i ", alreadyPlayedRandos[i]);
            OutputDebugString(buf);
        }
        OutputDebugString("\n");
    }

    alreadyPlayedRandos[alreadyPlayedCount++] = r;

    return allvids.get(r);
}

// int getUnplayedIndex()
// {
//     int randomCount = sizeof(RANDOM_VIDEOS) / sizeof(RANDOM_VIDEOS[0]);

//     // note alloc before we reduce so we always have enough space
//     if (!alreadyPlayedRandos)
//     {
//         alreadyPlayedRandos = (int*)malloc(sizeof(int) * randomCount);
//     }


//     // todo: better way to do this
//     // drop the xmas songs if not december
//     char month[32];
//     int res = GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, 0, "M", (char*)&month, 32);
//     if (month[1] != '2') // _2 = december
//     {
//         randomCount -= xmasCount;
//     }


//     if (alreadyPlayedCount >= randomCount) // we've gone through every video once
//     {
//         // keep our last video so we never get same twice in row
//         alreadyPlayedRandos[0] = alreadyPlayedRandos[alreadyPlayedCount-1];
//         alreadyPlayedCount = 1;
//     }

//     // todo: technically unbounded
//     int r = randomInt(randomCount);
//     while (alreadyPlayed(r))
//     {
//         r = randomInt(randomCount);
//     }

//     alreadyPlayedRandos[alreadyPlayedCount++] = r;

//     // outputAlreadyPlayedList();

//     return r;
// }
