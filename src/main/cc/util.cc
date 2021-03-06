/*
 Copyright (c) 2019-2020 Fredrik Öhrström

 MIT License

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#include "util.h"

#include <assert.h>
#include <string>
#include <string.h>
#include <map>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/error.h>
#endif

using namespace std;

void add_string(char *s, StringCount &c)
{
    assert(s != NULL);
    assert(s[0] != 0);

    //printf("add %s\n", s);
    char *p = s+1;

    int x = 0;
    while (*p != 0)
    {
        char tmp = *p;
        *p = 0;
        string tmps(s);
        *p = tmp;
        if (c.count(tmps) == 0)
        {
            c[tmps] = 1+x;
            //printf("Set \"%s\" = 1\n", tmps.c_str());
            break;
        }
        else
        {
            c[tmps]++;
            //printf("Inc \"%s\" = %d\n", tmps.c_str(), c[tmps]);
        }
        p++;
        x++;
    }
}

string find_prefix(char *s, StringCount &c)
{
    assert(s != NULL);
    assert(s[0] != 0);

    char *p = s+1;
    string prev;
    size_t prev_count = 0;

    while (*p != 0)
    {
        char tmp = *p;
        *p = 0;
        string tmps(s);
        *p = tmp;
        size_t count = 0;
        if (c.count(tmps) > 0) {
            count = c[tmps];
        }
        //printf("find \"%s\" %zu\n", tmps.c_str(), count);
        if (count < prev_count)
        {
            //printf("find done %s\n", prev.c_str());
            return prev;
        }
        prev = tmps;
        p++;
        prev_count = count;
    }
    return "";
}

bool loadFile(string file, vector<char> *buf)
{
    int blocksize = 1024;
    char block[blocksize];

    int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Could not open file %s errno=%d\n", file.c_str(), errno);
        return false;
    }
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Could not read file %s errno=%d\n", file.c_str(), errno);
            close(fd);

            return false;
        }
        buf->insert(buf->end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);
    return true;
}

bool loadStdin(vector<char> *buf)
{
    int blocksize = 1024;
    char block[blocksize];

    int fd = 0;
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Could not read stdin errno=%d\n", errno);
            close(fd);

            return false;
        }
        buf->insert(buf->end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);
    return true;
}

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

bool isValidUtf8(vector<char> *data, int *line, int *col)
{
    int n = 0; // Number of utf8 suffix bytes expected.
    *col = 0;
    *line = 1;
    for(size_t i = 0; i < data->size(); i++)
    {
        unsigned char c = (*data)[i];
        //printf("C " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(c));
        if (c == '\n')
        {
            *col = 0;
            (*line) ++;
        }

        if (n > 0)
        {
            // We are expecting suffix bytes
            if ((c >> 6) != 0b10) {
                //printf("err1\n");
                return false;
            }
            n--;
            //printf("now expecting %d\n", n);
            continue;
        }

        (*col)++;
        if ((c >> 5) == 0b110)
        {
            // The start byte with top bits 110 means we expect 1 suffix byte.
            //printf("new utf8 char expecting 1\n");
            n = 1;
            continue;
        }
        if ((c >> 4) == 0b1110)
        {
            // Top bits 1110 means expect 2 bytes.
            //printf("new utf8 char expecting 2\n");
            c = 2;
            continue;
        }
        if ((c >> 3) == 0b11110)
        {
            // Top bits 11110 means expect 3 bytes.
            //printf("new utf8 char expecting 3\n");
            n = 3;
            continue;
        }
        if ((c >> 7) == 1) {
            //printf("Bad top bit!\n");
            return false;
        }
    }
    //printf("n == %d\n", n);
    return n == 0;
}

bool removeCrs(vector<char> *data)
{
    size_t i, j, n = data->size();

    for(i = j = 0; i < n; i++, j++)
    {
        if ((*data)[i] == '\r'
            && i+1 < n &&
            (*data)[i+1] == '\n')
        {
            i++;
        }
        if (i > j)
        {
            (*data)[j] = (*data)[i];
        }
    }

    n = i-j;

    while (n--) data->pop_back();

    return i > j;
}
