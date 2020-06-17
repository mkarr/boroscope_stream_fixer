/*
MIT License

Copyright (c) 2020 Michael Andrew Karr, IV.

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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#ifdef DEBUG
#define DEBUG_PRINT 1
#else
#define DEBUG_PRINT 0
#endif
#define debug_print(...)                  \
    do                                    \
    {                                     \
        if (DEBUG_PRINT)                  \
            fprintf(stderr, __VA_ARGS__); \
    } while (0)

char frame_start_tag[9] = {'B', 'o', 'u', 'n', 'd', 'a', 'r', 'y', 'S'};
char frame_end_tag[9] = {'B', 'o', 'u', 'n', 'd', 'a', 'r', 'y', 'E'};

// Frame header is little endian. So this should just work on Intel machines.
// Only know what some of these fields are, but really only size matters.
// The dimensions seem to be incorrect, anyways.

struct __attribute__((__packed__)) frame_header
{
    u_int8_t p1;
    u_int8_t p2;
    u_int8_t p3;
    u_int8_t p4;
    u_int32_t size;
    u_int32_t p6;
    u_int32_t p7;
    u_int32_t p8;
    u_int16_t p9;
    u_int16_t p10;
    u_int32_t width;
    u_int32_t height;
};

struct fd_stream
{
    FILE *fd;
    int server_sock, client_sock;
};

struct tcp_addr
{
    char *addr;
    char *port;
};

int parse_addr(struct tcp_addr *addr, char *str)
{
    char *str_copy = strdup(str);
    char *str_copy_orig = str_copy;
    char *tmp;
    tmp = strsep(&str_copy, ":");
    // Don't need to save the 'tcp' string.
    tmp = strsep(&str_copy, ":");
    addr->addr = strdup(tmp);
    tmp = strsep(&str_copy, ":");
    addr->port = strdup(tmp);
    free(str_copy_orig);

    if ((addr->addr == NULL) || (addr->port == NULL))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int open_input_stream(struct fd_stream *file_in, char *in)
{
    struct tcp_addr addr;
    int sock;
    struct sockaddr_in server;
    struct hostent *host;

    if (strstr(in, "tcp:"))
    {
        if (parse_addr(&addr, in) == 0)
        {
            return 0;
        }
        else
        {
            debug_print("Network Input Stream: address = %s, port = %s\n", addr.addr, addr.port);

            host = gethostbyname(addr.addr);

            if (host == NULL)
            {
                fprintf(stderr, "Error: failed to gethostbyname() - %s\n", hstrerror(h_errno));
                return 0;
            }

            sock = socket(AF_INET, SOCK_STREAM, 0);

            if (sock == -1)
            {
                fprintf(stderr, "Error: failed to socket() - %s\n", strerror(errno));
                return 0;
            }

            server.sin_addr = *((struct in_addr **)host->h_addr_list)[0];
            server.sin_family = AF_INET;
            server.sin_port = htons(atoi(addr.port));

            if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1)
            {
                fprintf(stderr, "Error: failed to connect() - %s\n", strerror(errno));
                return 0;
            }

            file_in->fd = fdopen(sock, "r");

            if (file_in->fd == NULL)
            {
                fprintf(stderr, "Error: failed to fdopen() - %s\n", strerror(errno));
                return 0;
            }
        }
    }
    else
    {
        file_in->fd = fopen(in, "r");

        if (file_in->fd == NULL)
        {
            fprintf(stderr, "Error: failed to fopen() - %s\n", strerror(errno));
            return 0;
        }
    }

    return 1;
}

int open_output_stream(struct fd_stream *file_out, char *out)
{
    struct tcp_addr addr;
    int bind_sock, write_sock;
    struct sockaddr_in server, client;
    size_t sockaddr_in_size = sizeof(struct sockaddr_in);

    if (strstr(out, "tcp:"))
    {
        if (parse_addr(&addr, out) == 0)
        {
            return 0;
        }
        else
        {
            debug_print("Network Output Stream: address = %s, port = %s\n", strlen(addr.addr) > 0 ? addr.addr : "EMPTY", addr.port);

            // Does not makes sense to dns lookup output hostname. Just assume it is an IP.

            bind_sock = socket(AF_INET, SOCK_STREAM, 0);

            if (bind_sock == -1)
            {
                fprintf(stderr, "Error: failed to socket() - %s\n", strerror(errno));
                return 0;
            }

            file_out->server_sock = bind_sock;

            if (setsockopt(bind_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
            {
                fprintf(stderr, "Error: failed to setsockopt() - %s\n", strerror(errno));
                return 0;
            }

            // If IP is empty, just bind to default.

            server.sin_addr.s_addr = strlen(addr.addr) > 0 ? inet_addr(addr.addr) : INADDR_ANY;
            server.sin_family = AF_INET;
            server.sin_port = htons(atoi(addr.port));

            if (bind(bind_sock, (struct sockaddr *)&server, sizeof(server)) == -1)
            {
                fprintf(stderr, "Error: failed to bind() - %s\n", strerror(errno));
                return 0;
            }

            listen(bind_sock, 1);

            write_sock = accept(bind_sock, (struct sockaddr *)&client, (socklen_t *)&sockaddr_in_size);

            if (write_sock == -1)
            {
                fprintf(stderr, "Error: failed to accept() - %s\n", strerror(errno));
                return 0;
            }

            file_out->client_sock = write_sock;
            file_out->fd = fdopen(write_sock, "w");

            if (file_out->fd == NULL)
            {
                fprintf(stderr, "Error: failed to fdopen() - %s\n", strerror(errno));
                return 0;
            }
        }
    }
    else
    {
        file_out->fd = fopen(out, "w");

        if (file_out->fd == NULL)
        {
            fprintf(stderr, "Error: failed to fopen() - %s\n", strerror(errno));
            return 0;
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    int opt;
    struct fd_stream in, out;
    char *in_str, *out_str;
    int log_frames = 0;
    int c;
    int match_index = 0;
    struct frame_header header;
    char *buf;
    size_t buf_size;

    buf_size = 16384; // This should be good for most frames.
    buf = malloc(buf_size);

    int i;

    while ((opt = getopt(argc, argv, "i:o:hl")) != -1)
    {
        switch (opt)
        {
        case 'i':
            in_str = optarg;
            break;
        case 'o':
            out_str = optarg;
            break;
        case 'l':
            log_frames = 1;
            break;
        case 'h':
            fprintf(stderr, "Usage: [-l] [-i file|tcp:addr:port] [-o file|tcp:addr:port]\n");
            return 1;
            break;
        }
    }

    if (in_str == NULL)
    {
        in.fd = stdin;
    }
    else
    {
        debug_print("Input stream = %s\n", in_str);
        if (open_input_stream(&in, in_str) == 0)
        {
            fprintf(stderr, "Error: Failed to open input on stream '%s'.\n", in_str);
            return 1;
        }
    }

    if (out_str == NULL)
    {
        out.fd = stdout;
    }
    else
    {
        debug_print("Output stream = %s\n", out_str);
        if (open_output_stream(&out, out_str) == 0)
        {
            fprintf(stderr, "Error: Failed to open output on stream '%s'.\n", out_str);
            return 1;
        }
    }

    while ((c = getc(in.fd)) != EOF)
    {
        if (c == frame_start_tag[match_index])
        {
            match_index++;
            if (match_index == sizeof(frame_start_tag))
            {
                // Found frame start tag.

                match_index = 0;

                // Read in frame header.

                for (i = 0; i < sizeof(struct frame_header); i++)
                {
                    ((char *)&header)[i] = getc(in.fd);
                }

                if (log_frames)
                {
                    fprintf(stderr, "frame: size = %d, width = %d, height = %d\n", header.size, header.width, header.height);
                }

                // Read in mjpeg frame.

                if (header.size > buf_size)
                {
                    free(buf);
                    buf_size = header.size;

                    // Find next power of 2 > size.

                    buf_size--;
                    buf_size |= buf_size >> 1;
                    buf_size |= buf_size >> 2;
                    buf_size |= buf_size >> 4;
                    buf_size |= buf_size >> 8;
                    buf_size |= buf_size >> 16;
                    buf_size++;

                    buf = malloc(buf_size);

                    debug_print("Reallocated buffer: size = %ld\n", buf_size);
                }

                for (i = 0; i < header.size; i++)
                {
                    buf[i] = getc(in.fd);
                };

                // "Unencrypt" frame.

                buf[header.size / 2] = ~buf[header.size / 2];

                // Dump unencrypted mjpeg frame sans header/footer.

                for (i = 0; i < header.size; i++)
                {
                    putc(buf[i], out.fd);
                };

                // Read in end tag;

                for (i = 0; i < sizeof(frame_start_tag); i++)
                {
                    getc(in.fd);
                };
            }
        }
        else
        {
            match_index = 0;
        }
    }

    return 0;
}
