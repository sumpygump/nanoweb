#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define VERSION   23
#define BUFSIZE   8096
#define ERROR     42
#define LOG       44
#define FORBIDDEN 403
#define NOTFOUND  404

/**
 * Nanoweb - a small web server written in C.
 *
 * @author Nigel Griffiths <nag@uk.ibm.com>
 * @author Jansen Price <jansen.price@gmail.com>
 */

/**
 * Extension and mimetype mapping
 */
struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpg" },
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"ico", "image/ico" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {"css", "text/css" },
    {"js",  "text/javascript" },
    {"json","application/json" },
    {"woff","application/font-woff" },
    {"ttf", "application/font-ttf" },
    {"svg", "application/svg" },
    {0,0} };

/**
 * Log Message
 *
 * @param int type The log type
 * @param char* label Label
 * @param char* message Message
 * @param int socket_fd count or PID
 */
void log_message(int type, char *label, char *message, int socket_fd)
{
    int fd;
    char logbuffer[BUFSIZE*2];

    switch (type) {
    case ERROR:
        (void) sprintf(logbuffer, "[ERROR] %d %s:%s Errno=%d exiting pid=%d", getpid(), label, message, errno, getpid());
        (void) fprintf(stderr, "[ERROR] %d %s:%s Errno=%d\n", getpid(), label, message, errno);
        break;
    case FORBIDDEN:
        (void) write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
        (void) sprintf(logbuffer, "[FORBIDDEN] %d %s:%s", getpid(), label, message);
        break;
    case NOTFOUND:
        (void) write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n", 224);
        (void) sprintf(logbuffer, "[NOT FOUND] %d %s:%s", getpid(), label, message);
        break;
    case LOG:
        (void) sprintf(logbuffer, "[INFO] %d %s:%s:%d", getpid(), label, message, socket_fd);
        break;
    }

    /* No checks here, nothing can be done with a failure anyway */
    if ((fd = open("nanoweb.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        (void)write(fd, logbuffer, strlen(logbuffer));
        (void)write(fd, "\n", 1);
        (void)close(fd);
    }

    if (type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit)
{
    int j, file_fd, buflen;
    long i, ret, len;
    char * fstr;
    static char buffer[BUFSIZE+1]; /* static so zero filled */

    ret = read(fd, buffer, BUFSIZE); /* read Web request in one go */

    if (ret == 0 || ret == -1) { /* read failure stop now */
        log_message(FORBIDDEN, "failed to read browser request", "", fd);
    }

    if (ret > 0 && ret < BUFSIZE) { /* return code is valid chars */
        buffer[ret] = 0; /* terminate the buffer */
    } else {
        buffer[0] = 0;
    }

    for (i = 0; i <ret; i++) { /* remove CF and LF characters */
        if (buffer[i] == '\r' || buffer[i] == '\n') {
            buffer[i] = '*';
        }
    }

    log_message(LOG, "request", buffer, hit);

    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
        log_message(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    }

    for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
        if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
            buffer[i] = 0;
            break;
        }
    }

    for (j = 0; j < i - 1; j++) { /* check for illegal parent directory use .. */
        if (buffer[j] == '.' && buffer[j + 1] == '.') {
            log_message(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
        }
    }

    if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) {
        /* convert no filename to index file */
        (void)strcpy(buffer,"GET /index.html");
    }

    /* work out the file type and check we support it */
    buflen = strlen(buffer);
    fstr = (char *)0;

    for (i = 0; extensions[i].ext != 0; i++) {
        len = strlen(extensions[i].ext);
        if (!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
            fstr =extensions[i].filetype;
            break;
        }
    }
    
    if (fstr == 0) {
        log_message(FORBIDDEN, "file extension type not supported", buffer, fd);
    }

    if ((file_fd = open(&buffer[5], O_RDONLY)) == -1) { /* open the file for reading */
        log_message(NOTFOUND, "failed to open file", &buffer[5], fd);
    }

    log_message(LOG, "SEND", &buffer[5], hit);
    len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */

    (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
    (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nanoweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */

    log_message(LOG, "Header", buffer, hit);
    (void)write(fd, buffer, strlen(buffer));

    /* send file in 8KB block - last block may be smaller */
    while ((ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
        (void)write(fd, buffer, ret);
    }

    sleep(1); /* allow socket to drain before signalling the socket is closed */
    close(fd);
    exit(1);
}

int main(int argc, char **argv)
{
    int i, port, pid, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */

    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
        (void)printf("Nanoweb version %d\nusage: nanoweb <port-number> <root-directory>\n\n"
            "  Nanoweb is a small and very safe mini web server\n"
            "  nanoweb only serves files and web pages with extensions named below\n"
            "  and only from the named directory or its sub-directories.\n"
            "  There are no fancy features = safe and secure.\n\n"
            "  Example: nanoweb 8181 /home/nwebdir\n\n", VERSION
        );

        (void)printf("  Only Supports:");

        for (i = 0; extensions[i].ext != 0; i++) {
            (void)printf(" %s", extensions[i].ext);
        }

        (void)printf("\n  Not Supported: URLs including \"..\", Java, Javascript, CGI\n"
            "  Not Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
            "  No warranty given or implied\n  Nigel Griffiths nag@uk.ibm.com\n"
        );

        exit(0);
    }

    port = atoi(argv[1]);
    if (port < 0 || port > 60000) {
        (void)printf("ERROR: Invalid port number %s (try between 1 and 60000)\n", argv[1]);
        exit(2);
    }

    if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5)
        || !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5)
        || !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5)
        || !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)
    ) {
        (void)printf("ERROR: Bad root directory %s, see nanoweb -?\n", argv[2]);
        exit(3);
    }

    if (chdir(argv[2]) == -1){
        (void)printf("ERROR: Cannot change to directory '%s'\n", argv[2]);
        exit(4);
    }

    // Become deamon + unstopable and no zombies children (= no wait())
    /*if (fork() != 0) {
        return 0; // parent returns OK to shell
    }*/

    (void)printf("Starting nanoweb server on port %s, root directory '%s', PID %d\n", argv[1], argv[2], getpid());

    (void)signal(SIGCHLD, SIG_IGN); /* ignore child death */
    (void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */

    (void)setpgrp(); /* break away from process group */
    log_message(LOG, "Starting nanoweb server on port ", argv[1], getpid());

    for (i = 0; i < 32; i++) {
        (void)close(i); /* close open files */
    }

    /* setup the network socket */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message(ERROR, "system call", "Error creating network socket", 0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        log_message(ERROR, "system call", "Error binding to port", 0);
    }

    if (listen(listenfd, 64) < 0) {
        log_message(ERROR, "system call", "Error listening to socket", 0);
    }

    for (hit = 1; ; hit++) {
        length = sizeof(cli_addr);

        if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) {
            log_message(ERROR, "system call", "accept", 0);
        }

        fflush(0);
        if ((pid = fork()) < 0) {
            log_message(ERROR, "system call", "fork", 0);
        } else {
            if (pid == 0) { /* child */
                (void)close(listenfd);
                web(socketfd, hit); /* never returns */
            } else {     /* parent */
                (void)close(socketfd);
            }
        }
    }
}
