#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include <bee.h>
#include <db.h>
#include <res.h>
#include <links.h>
#include <logidx.h>

#define ONE_DAY                 (3600 * 24)
#define DEFAULT_BUFFER_RECS     (16384)

char      * logname = "/var/bee/beelog.dat";     // bee log filename
char      * idxname = "/var/bee/beelog.idx";     // bee log index filename

logbase_t   Logbase;
long long   recs;

char      * first_file;
char      * second_file;

logrec_t    logrec;

idxhead_t   idxhead;
long long   idxstart    = 0;

size_t      bufferSize = DEFAULT_BUFFER_RECS * sizeof(logrec_t);

int         verbose = 0;

// Function prototypes
time_t  parse_time(char * str);
void    usage();
char  * strtime(time_t utc);

int main (int argc, char **argv)
{
    long long rc;
    long long i;

    char *ptr = NULL;

    int fd = (-1);

    time_t  splitOnTime = 0;

    logrec_t  * iobuffer;

    long long   start_offsets[2];
    long long   end_offsets[2];

    int         fd_in;
    int         fd_out[2];
    long long   bytesRemaining = 0;
    long long   bytesToRead    = 0;

    int         isFirst = 0;
    time_t      checkTime;

// Set config defaults
    rc = conf_load (NULL);
    if (rc == 0) {
        logname = conf_logfile;
        idxname = conf_logindex;
    }

#define PARAMS "f:F:i:o:vz:"

    while ((rc = getopt (argc, argv, PARAMS)) != -1) {
        switch (rc) {
            case 'F':
                splitOnTime = parse_time (optarg);
                break;

            case 'f':
                logname = optarg;
                break;

            case 'i':
                idxname = optarg;
                break;

            case 'v':
                verbose = 1;
                break;

            case 'o':
                ptr = optarg;
                first_file  = strsep(&ptr, ",");
                if (first_file)
                    second_file = strsep(&ptr, ",");
                break;

            case 'z':
                bufferSize = strtoul(optarg, NULL, 10) * sizeof(logrec_t);
                if (bufferSize == 0) {
                    fprintf(stderr, "Error: invalid buffer size\n");
                    return EXIT_FAILURE;
                }
                break;

            default:
                usage();
                exit (EXIT_FAILURE);
        }
    }

    if (!first_file || !second_file || !splitOnTime) {
        usage();
        exit (EXIT_FAILURE);
    }

    if (verbose) {
        fprintf(stderr, "Input file   - %s\n", logname);
        fprintf(stderr, "Index file   - %s\n", idxname);
        fprintf(stderr, "Split offset - %s\n", strtime(splitOnTime));
    }

// Find rough position via index
    while (1) {  // fictive cycle
        fd = open (idxname, O_RDONLY | O_SHLOCK, 0644);
        if (fd < 0) {
            if (verbose)
                fprintf(stderr, "Warning: open(%s): %s\n", idxname, strerror(errno));
            break;
        }
        rc = read (fd, &idxhead, sizeof(idxhead));
        if (rc < sizeof (idxhead)) {
            if (verbose)
                fprintf(stderr, "Warning: read(idxhead): %s\n", strerror(errno));
            close(fd);
            break;
        }

    // check file magic
        if (memcmp (idxhead.marker, IDXMARKER, sizeof (idxhead.marker)) != 0) {
            if (verbose)
                fprintf(stderr, "Warning: incorrect index magic\n");
            close(fd);
            break;
        }

        if (verbose)
            fprintf(stderr, "Index starts at %s\n", strtime(idxhead.first));

    // count offset to index 2 days before (index could have 1-hour inaccurancy, etc.)
        i = ((splitOnTime - idxhead.first) / ONE_DAY - 2) * sizeof(idxstart)
                + sizeof(idxhead);

    // read indexed record number
        if (i > 0) {
            rc = lseek (fd, i, SEEK_SET);
            if (rc == i) {
                rc = read (fd, &idxstart, sizeof (idxstart));
                if (rc != sizeof (idxstart)) {
                    idxstart = 0;
                    fprintf(stderr, "Warning: No such time in the index\n");
                }
            }
        }
        close (fd);
        fd = (-1);
        break;
    }

    if (verbose)
        fprintf(stderr, "Starting search at record #%lld ", idxstart);

// Now search for exact split position
    rc = log_baseopen_sr (&Logbase, logname);
    if (rc < 0) {
        fprintf(stderr, "main(log_baseopen) : Error\n");
        return (-1);
    }

    memset(&logrec, 0 , sizeof(logrec));

    if (verbose) {
        if (logi_get(&Logbase, idxstart, &logrec) == SUCCESS)
            fprintf(stderr, "(%s)\n", strtime(logrec.time));
        else
            fprintf(stderr, "(N/A)\n");
    }

    do {
        rc = logi_get(&Logbase, idxstart++, &logrec);
        if (rc != SUCCESS) {
            if (rc == NOT_FOUND) {
                fprintf(stderr, "Given time is too big, last - %s\n", strtime(logrec.time));
            }
            else {
                fprintf(stderr, "Error: Search error\n");
            }
            return (EXIT_FAILURE);
        }
    } while (logrec.time < splitOnTime);
    idxstart--;

    if (idxstart == 0) {
        fprintf(stderr, "Given time is too small, first - %s\n", strtime(logrec.time));
        return (EXIT_FAILURE);
    }

    if (verbose)
        fprintf(stderr, "found %s at record #%lld\n", strtime(logrec.time), idxstart);

    checkTime = logrec.time;

    recs = log_reccount(&Logbase);

    if (verbose)
        fprintf(stderr, "total recs count %lld\n", recs);

    log_baseclose(&Logbase);

// Prepare to copy
    iobuffer = calloc(1, bufferSize);

    start_offsets[0] = 0;
    end_offsets  [0] = idxstart * sizeof(logrec_t);
    if (end_offsets[0] < start_offsets[0])
        end_offsets[0] = start_offsets[0];

    start_offsets[1] = idxstart * sizeof(logrec_t);
    end_offsets  [1] = recs     * sizeof(logrec_t);
    if (end_offsets[1] < start_offsets[1])
        end_offsets[1] = start_offsets[1];

    if (verbose) {
        fprintf(stderr, "Copy plan:\n");
        fprintf(stderr, "%s: %lld-%lld\n", first_file,  start_offsets[0], end_offsets[0] - 1);
        fprintf(stderr, "%s: %lld-%lld\n", second_file, start_offsets[1], end_offsets[1] - 1);
    }

    fd_in     = open(logname, O_RDONLY);
    if (fd_in < 0) {
        fprintf(stderr, "Error: open(%s): %s\n", logname, strerror(errno));
        return EXIT_FAILURE;
    }

    fd_out[0] = open(first_file,  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_out[0] < 0) {
        fprintf(stderr, "Error: open(%s): %s\n", first_file, strerror(errno));
        return EXIT_FAILURE;
    }

    fd_out[1] = open(second_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_out[0] < 0) {
        fprintf(stderr, "Error: open(%s): %s\n", second_file, strerror(errno));
        return EXIT_FAILURE;
    }

// Now lets copy data
    for (i=0; i < 2; i++) {
        bytesRemaining = end_offsets[i] - start_offsets[i];
        isFirst = 1;

        while (bytesRemaining > 0) {
            bytesToRead = bytesRemaining;
            if (bytesToRead > bufferSize)
                bytesToRead = bufferSize;

            if (read(fd_in, iobuffer, bytesToRead) != bytesToRead
                    || write(fd_out[i], iobuffer, bytesToRead) != bytesToRead) {
                fprintf(stderr, "Error: file i/o error\n");
                return (EXIT_FAILURE);
            }

            if (isFirst) {
                isFirst = 0;
                if (verbose) {
                    fprintf(stderr, "file %lld, first record - %s\n", i,
                            strtime(iobuffer[0].time));
                }

            // consistency check
                if (i > 0 && iobuffer[0].time != checkTime) {
                    fprintf(stderr, "Bug: timestamp mismatch\n");
                    return (EXIT_FAILURE);
                }
            }

            bytesRemaining -= bytesToRead;
        }
        close(fd_out[i]);

        if (verbose) {
            fprintf(stderr, "file %lld, last record - %s\n", i,
                    strtime(iobuffer[(bytesToRead/sizeof(logrec_t))-1].time));
        }
    }
    close(fd_in);

    return 0;
}

// 1.01.2002.20.53.42

char delim[] = " \n\t:./\\;,'\"`-";

time_t parse_time (char *strdate)
{
    char *ptr = strdate;
    char *str;
    struct tm stm;
    time_t rval;
    int temp;
    int eol = 0;

    memset (&stm, 0, sizeof (stm));

// month day 
    str = next_token (&ptr, delim);
    if (str == NULL)
        return 0;
    stm.tm_mday = strtol (str, NULL, 10);
    if (stm.tm_mday > 1000000000) {
        str = next_token (&ptr, delim);
        if (str != NULL)
            return 0;
        return stm.tm_mday;     // return raw UTC
    }
// month
    str = next_token (&ptr, delim);
    if (str == NULL)
        return 0;
    stm.tm_mon = strtol (str, NULL, 10) - 1;
// year
    str = next_token (&ptr, delim);
    if (str == NULL)
        return 0;
    temp = strtol (str, NULL, 10);
    if (temp < 100)
        temp += 100;
    else
        temp -= 1900;
    if (temp < 0)
        return 0;
    stm.tm_year = temp;
// hour (optional)
    str = next_token (&ptr, delim);
    if (str == NULL)
        eol = 1;
    else
        stm.tm_hour = strtol (str, NULL, 10);
// minute (optional)
    if (!eol) {
        str = next_token (&ptr, delim);
        if (str == NULL)
            eol = 1;
        else
            stm.tm_min = strtol (str, NULL, 10);
    }
// seconds (optional)
    if (!eol) {
        str = next_token (&ptr, delim);
        if (str == NULL)
            eol = 1;
        else
            stm.tm_sec = strtol (str, NULL, 10);
    }
// assemble time_t
    stm.tm_isdst = -1;
    rval = mktime (&stm);
    if (rval < 0)
        return 0;

    return rval;
}

char * strtime (time_t utc)
{
    struct tm stm;
    static char tbuf[64];

    if (localtime_r (&utc, &stm) == NULL)
        return "n/a";

    snprintf (tbuf, sizeof (tbuf), "%02d:%02d:%04d %02d:%02d:%02d",
              stm.tm_mday, stm.tm_mon + 1, stm.tm_year + 1900,
              stm.tm_hour, stm.tm_min, stm.tm_sec);

    return tbuf;
}

void usage ()
{
    fprintf (stderr,
             "BEE - Small billing solutions project ver. 0.9.10+\n"
             "   Transaction log split utility\n\n"
             "BSD licensed, see LICENSE for details. OpenBSD.RU project\n\n"
             " Usage:\n"
             "   beelog-split [options] -o FILE1,FILE2 -F <split_time> \n"
             "      Available options:\n"
             " -F D:M:Y[h:m[:s]] - split time\n"
             "      or\n"
             " -F UTCseconds     - split time (seconds since Epoch)\n"
             " -f FILE           - input bee log file (default - configured one)\n"
             " -i FILE           - bee log index file (default - configured one)\n"
             " -o FILE1,FILE2    - output files\n"
             " -z RECS           - number of recs in buffer (default - 16384)\n"
             " -v                - verbose, print debug information\n\n");
}
