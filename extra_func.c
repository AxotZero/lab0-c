#include <arpa/inet.h> /* inet_ntoa */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#include "extra_func.h"


void swap_ptr(struct list_head **a, struct list_head **b)
{
    struct list_head *tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Get list node by given index
 */
void swap_node(struct list_head *a, struct list_head *b)
{
    if (a == b)
        return;
    else if (a->next == b || b->next == a)  // if they are neighbors
    {
        // decide who is left and who is right
        struct list_head *l = a, *r = b;
        if (b->next == a) {
            r = a;
            l = b;
        }

        // swap
        l->next = r->next;
        r->prev = l->prev;
        l->prev->next = r;
        l->prev = r;
        r->next->prev = l;
        r->next = l;
    } else {
        a->next->prev = b;
        b->next->prev = a;
        a->prev->next = b;
        b->prev->next = a;
        swap_ptr(&a->next, &b->next);
        swap_ptr(&a->prev, &b->prev);
    }
}

/*
 * Get list node by given index
 */
struct list_head *get_node_by_idx(struct list_head *head, int idx)
{
    for (; idx > 0; --idx)
        head = head->next;
    return head;
}

/*
 * Shuffle the list with Fisher–Yates shuffle algorithm
 * Time Complexity: O(N^2)
 * Space Complexity: O(1)
 */
void q_shuffle(struct list_head *head)
{
    srand(time(NULL));
    struct list_head *curr = head->prev;
    struct list_head *curr_next = NULL;
    for (int i = q_size(head); i > 1; --i) {
        curr_next = curr->prev;
        swap_node(curr, get_node_by_idx(head, rand() % i));
        curr = curr_next;
    }
}

/*
 * Utilize extra space to shuffle with Fisher–Yates shuffle
 * Time Complexity: O(N)
 * Space Complexity: O(N)
 */
void q_shuffle_dp(struct list_head *head)
{
    srand(time(NULL));
    int len = q_size(head);

    // list space to store all addressed of the queue.
    struct list_head **dp = malloc(sizeof(struct list_head **) * len);
    struct list_head *curr = head->next;
    for (int i = 0; i < len; ++i, curr = curr->next)
        dp[i] = curr;

    // shuffle
    curr = head->prev;
    struct list_head *curr_prev = NULL, *tmp = NULL;
    for (int i = len; i > 1; --i) {
        curr_prev = curr->prev;
        // swap nodes
        tmp = dp[rand() % i];
        swap_node(curr, tmp);

        // swap addresses in the list space
        swap_ptr(&dp[i], &tmp);

        // move to next
        curr = curr_prev;
    }
    free(dp);
}

/*
 * Compare two element_t->value
 */
int cmp(const struct list_head *a, const struct list_head *b)
{
    return strcmp(container_of(a, element_t, list)->value,
                  container_of(b, element_t, list)->value);
}

/*
 * Returns a list organized in an intermediate format suited
 * to chaining of merge() calls: null-terminated, no reserved or
 * sentinel head node, "prev" links not maintained.
 */
static struct list_head *merge(struct list_head *a, struct list_head *b)
{
    struct list_head *head = NULL, **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

/*
 * Combine final list merge with restoration of standard doubly-linked
 * list structure.  This approach duplicates code from merge(), but
 * runs faster than the tidier alternatives of either a separate final
 * prev-link restoration pass, or maintaining the prev links
 * throughout.
 */
static void merge_final(struct list_head *head,
                        struct list_head *a,
                        struct list_head *b)
{
    struct list_head *tail = head;
    unsigned char count = 0;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(a, b) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    tail->next = b;
    do {
        /*
         * If the merge is highly unbalanced (e.g. the input is
         * already sorted), this loop may run many iterations.
         * Continue callbacks to the client even though no
         * element comparison is needed, so the client's cmp()
         * routine can invoke cond_resched() periodically.
         */
        if (unlikely(!++count))
            cmp(b, b);
        b->prev = tail;
        tail = b;
        b = b->next;
    } while (b);

    /* And the final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

/**
 * list_sort - sort a list
 * @priv: private data, opaque to list_sort(), passed to @cmp
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * The comparison function @cmp must return > 0 if @a should sort after
 * @b ("@a > @b" if you want an ascending sort), and <= 0 if @a should
 * sort before @b *or* their original order should be preserved.  It is
 * always called with the element that came first in the input in @a,
 * and list_sort is a stable sort, so it is not necessary to distinguish
 * the @a < @b and @a == @b cases.
 *
 * This is compatible with two styles of @cmp function:
 * - The traditional style which returns <0 / =0 / >0, or
 * - Returning a boolean 0/1.
 * The latter offers a chance to save a few cycles in the comparison
 * (which is used by e.g. plug_ctx_cmp() in block/blk-mq.c).
 *
 * A good way to write a multi-word comparison is::
 *
 *	if (a->high != b->high)
 *		return a->high > b->high;
 *	if (a->middle != b->middle)
 *		return a->middle > b->middle;
 *	return a->low > b->low;
 *
 *
 * This mergesort is as eager as possible while always performing at least
 * 2:1 balanced merges.  Given two pending sublists of size 2^k, they are
 * merged to a size-2^(k+1) list as soon as we have 2^k following elements.
 *
 * Thus, it will avoid cache thrashing as long as 3*2^k elements can
 * fit into the cache.  Not quite as good as a fully-eager bottom-up
 * mergesort, but it does use 0.2*n fewer comparisons, so is faster in
 * the common case that everything fits into L1.
 *
 *
 * The merging is controlled by "count", the number of elements in the
 * pending lists.  This is beautifully simple code, but rather subtle.
 *
 * Each time we increment "count", we set one bit (bit k) and clear
 * bits k-1 .. 0.  Each time this happens (except the very first time
 * for each bit, when count increments to 2^k), we merge two lists of
 * size 2^k into one list of size 2^(k+1).
 *
 * This merge happens exactly when the count reaches an odd multiple of
 * 2^k, which is when we have 2^k elements pending in smaller lists,
 * so it's safe to merge away two lists of size 2^k.
 *
 * After this happens twice, we have created two lists of size 2^(k+1),
 * which will be merged into a list of size 2^(k+2) before we create
 * a third list of size 2^(k+1), so there are never more than two pending.
 *
 * The number of pending lists of size 2^k is determined by the
 * state of bit k of "count" plus two extra pieces of information:
 *
 * - The state of bit k-1 (when k == 0, consider bit -1 always set), and
 * - Whether the higher-order bits are zero or non-zero (i.e.
 *   is count >= 2^(k+1)).
 *
 * There are six states we distinguish.  "x" represents some arbitrary
 * bits, and "y" represents some arbitrary non-zero bits:
 * 0:  00x: 0 pending of size 2^k;           x pending of sizes < 2^k
 * 1:  01x: 0 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * 2: x10x: 0 pending of size 2^k; 2^k     + x pending of sizes < 2^k
 * 3: x11x: 1 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * 4: y00x: 1 pending of size 2^k; 2^k     + x pending of sizes < 2^k
 * 5: y01x: 2 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * (merge and loop back to state 2)
 *
 * We gain lists of size 2^k in the 2->3 and 4->5 transitions (because
 * bit k-1 is set while the more significant bits are non-zero) and
 * merge them away in the 5->2 transition.  Note in particular that just
 * before the 5->2 transition, all lower-order bits are 11 (state 3),
 * so there is one list of each smaller size.
 *
 * When we reach the end of the input, we merge all the pending
 * lists, from smallest to largest.  If you work through cases 2 to
 * 5 above, you can see that the number of elements we merge with a list
 * of size 2^k varies from 2^(k-1) (cases 3 and 5 when x == 0) to
 * 2^(k+1) - 1 (second merge of case 5 when x == 2^(k-1) - 1).
 */
void linux_sort(struct list_head *head)
{
    struct list_head *list = head->next, *pending = NULL;
    size_t count = 0; /* Count of pending */

    if (list == head->prev) /* Zero or one elements */
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    /*
     * Data structure invariants:
     * - All lists are singly linked and null-terminated; prev
     *   pointers are not maintained.
     * - pending is a prev-linked "list of lists" of sorted
     *   sublists awaiting further merging.
     * - Each of the sorted sublists is power-of-two in size.
     * - Sublists are sorted by size and age, smallest & newest at front.
     * - There are zero to two sublists of each size.
     * - A pair of pending sublists are merged as soon as the number
     *   of following pending elements equals their size (i.e.
     *   each time count reaches an odd multiple of that size).
     *   That ensures each later final merge will be at worst 2:1.
     * - Each round consists of:
     *   - Merging the two sublists selected by the highest bit
     *     which flips when count is incremented, and
     *   - Adding an element from the input as a size-1 sublist.
     */
    do {
        size_t bits;
        struct list_head **tail = &pending;

        /* Find the least-significant clear bit in count */
        for (bits = count; bits & 1; bits >>= 1)
            tail = &(*tail)->prev;
        /* Do the indicated merge */
        if (likely(bits)) {
            struct list_head *a = *tail, *b = a->prev;

            a = merge(b, a);
            /* Install the merged result in place of the inputs */
            a->prev = b->prev;
            *tail = a;
        }

        /* Move one element from input list to pending */
        list->prev = pending;
        pending = list;
        list = list->next;
        pending->next = NULL;
        count++;
    } while (list);

    /* End of input; merge together all the pending lists. */
    list = pending;
    pending = pending->prev;
    for (;;) {
        struct list_head *next = pending->prev;

        if (!next)
            break;
        list = merge(pending, list);
        pending = next;
    }
    /* The final merge, rebuilding prev links */
    merge_final(head, pending, list);
}


/*
 * Following code are for web
 */
int listenfd = -1;
bool noise = true;

#define LISTENQ 1024 /* second argument to listen() */
#define MAXLINE 1024 /* max length of a line */
#define RIO_BUFSIZE 1024

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 9999 /* use this port if none given as arg to main() */
#endif

#ifndef FORK_COUNT
#define FORK_COUNT 4
#endif

#ifndef NO_LOG_ACCESS
#define LOG_ACCESS
#endif


typedef struct {
    int rio_fd;                /* descriptor for this buf */
    int rio_cnt;               /* unread byte in this buf */
    char *rio_bufptr;          /* next unread byte in this buf */
    char rio_buf[RIO_BUFSIZE]; /* internal buffer */
} rio_t;

typedef struct sockaddr SA;

typedef struct {
    char filename[512];
    off_t offset; /* for support Range */
    size_t end;
} http_request;

typedef struct {
    const char *extension;
    const char *mime_type;
} mime_map;

// https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types

mime_map meme_types[] = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".arc", "application/x-freearc"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gz", "application/gzip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".mid", "audio/midi audio/x-midi"},
    {".midi", "audio/midi audio/x-midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mp4", "video/mp4"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".opus", "audio/opus"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".php", "application/x-httpd-php"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx",
     "application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar", "application/vnd.rar"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".swf", "application/x-shockwave-flash"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ts", "video/mp2t"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml", "text/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"},
    {NULL, NULL},
};

char *default_mime_type = "text/plain";

void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}


/*
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;
    while (rp->rio_cnt <= 0) { /* refill if buf is empty */

        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0) {
            if (errno != EINTR) { /* interrupted by sig handler return */
                return -1;
            }
        } else if (rp->rio_cnt == 0) { /* EOF */
            return 0;
        } else
            rp->rio_bufptr = rp->rio_buf; /* reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if (rp->rio_cnt < n) {
        cnt = rp->rio_cnt;
    }
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

/*
 * rio_readlineb - robustly read a text line (buffered)
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        int rc;
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n') {
                break;
            }
        } else if (rc == 0) {
            if (n == 1) {
                return 0; /* EOF, no data read */
            } else {
                break; /* EOF, some data was read */
            }
        } else {
            return -1; /* error */
        }
    }
    *bufp = 0;
    return n;
}

static const char *get_mime_type(char *filename)
{
    char *dot = strrchr(filename, '.');
    if (dot) {  // strrchar Locate last occurrence of character in string
        mime_map *map = meme_types;
        while (map->extension) {
            if (strcmp(map->extension, dot) == 0) {
                return map->mime_type;
            }
            map++;
        }
    }
    return default_mime_type;
}

int open_listenfd()
{
    int optval = 1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
                   sizeof(int)) < 0) {
        return -1;
    }

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s
    if (setsockopt(listenfd, 6, TCP_CORK, (const void *) &optval, sizeof(int)) <
        0) {
        return -1;
    }

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) DEFAULT_PORT);
    if (bind(listenfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0) {
        return -1;
    }

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0) {
        return -1;
    }
#ifdef LOG_ACCESS
    printf("listen on port %d, fd is %d\n", DEFAULT_PORT, listenfd);
#endif
    return listenfd;
}

void url_decode(char *src, char *dest, int max)
{
    char *p = src;
    char code[3] = {0};
    while (*p && --max) {
        if (*p == '%') {
            memcpy(code, ++p, 2);
            *dest++ = (char) strtoul(code, NULL, 16);
            p += 2;
        } else {
            *dest++ = *p++;
        }
    }
    *dest = '\0';
}

void parse_request(int fd, http_request *req)
{
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE];
    req->offset = 0;
    req->end = 0; /* default */

    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%1023s %1023s", method, uri); /* version is not cared */
    /* read all */
    while (buf[0] != '\n' && buf[1] != '\n') { /* \n || \r\n */
        rio_readlineb(&rio, buf, MAXLINE);
        if (buf[0] == 'R' && buf[1] == 'a' && buf[2] == 'n') {
            sscanf(buf, "Range: bytes=%lu-%lu", (unsigned long *) &req->offset,
                   (unsigned long *) &req->end);
            // Range: [start, end]
            if (req->end != 0) {
                req->end++;
            }
        }
    }
    char *filename = uri;
    if (uri[0] == '/') {
        filename = uri + 1;
        int length = strlen(filename);
        if (length == 0) {
            filename = ".";
        } else {
            int i = 0;
            for (; i < length; ++i) {
                if (filename[i] == '?') {
                    filename[i] = '\0';
                    break;
                }
            }
        }
    }
    url_decode(filename, req->filename, MAXLINE);
}

#ifdef LOG_ACCESS
void log_access(int status, struct sockaddr_in *c_addr, http_request *req)
{
    printf("%s:%d %d - '%s' (%s)\n", inet_ntoa(c_addr->sin_addr),
           ntohs(c_addr->sin_port), status, req->filename,
           get_mime_type(req->filename));
}
#endif

char *process(int fd, struct sockaddr_in *clientaddr)
{
#ifdef LOG_ACCESS
    printf("accept request, fd is %d, pid is %d\n", fd, getpid());
#endif
    http_request req;
    parse_request(fd, &req);
    int status = 200;

    char *p = req.filename;
    /* Change '/' to ' ' */
    while (*p) {
        ++p;
        if (*p == '/') {
            *p = ' ';
        }
    }
#ifdef LOG_ACCESS
    log_access(status, clientaddr, &req);
#endif
    char *ret = malloc(strlen(req.filename) + 1);
    strncpy(ret, req.filename, strlen(req.filename) + 1);

    return ret;
}