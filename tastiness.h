#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include "fceux/src/driver.h"
#include "fceux/src/drivers/common/args.h"
#include "fceux/src/drivers/common/cheat.h"
#include "fceux/src/fceu.h"
#include "fceux/src/types.h"
#include "fceux/src/utils/md5.h"
#include "fceux/src/movie.h"
#include "fceux/src/version.h"
#include "fceux/src/state.h"

#include "city.h"

#include "emulator.h"
#include "debug.h"

inline uint64_t microTime() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return uint64_t(tv.tv_sec)*1000000 + uint64_t(tv.tv_usec);
}

