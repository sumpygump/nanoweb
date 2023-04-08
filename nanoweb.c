#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VERSION   24
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
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".gif", "image/gif"},
    {".gz",  "image/gz"},
    {".html", "text/html"},
    {".htm", "text/html"},
    {".ico", "image/ico"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpg"},
    {".json", "application/json"},
    {".js",  "text/javascript"},
    {".log", "text/plain"},
    {".mp3", "audio/mpeg"},
    {".ogg", "audio/ogg"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".svg", "image/svg+xml"},
    {".tar", "image/tar"},
    {".ttf", "application/font-ttf"},
    {".txt", "text/plain"},
    {".wav", "audio/wav"},
    {".woff", "application/font-woff"},
    {".zip", "image/zip"},
    {0, 0}
};

/**
 * Log Message
 *
 * @param int type The log type
 * @param char* label Label
 * @param char* message Message
 * @param int socket_fd count or PID
 */
void log_message(int type, char *label, char *message, int socket_fd) {
    int fd;
    char logbuffer[BUFSIZE * 2];

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

    if (type == ERROR || type == NOTFOUND || type == FORBIDDEN) {
        exit(3);
    }
}

/* Function: url_decode */
char *url_decode(const char *str) {
    int d = 0; /* whether or not the string is decoded */

    char *dStr = malloc(sizeof(char) * (strlen(str) + 24));
    char eStr[] = "00"; /* for a hex code */

    strcpy(dStr, str);

    while (!d) {
        d = 1;
        int i; /* the counter for the string */

        for (i = 0; i < strlen(dStr); ++i) {
            if (dStr[i] == '+') {
                dStr[i] = ' ';
            }
            if (dStr[i] == '%') {
                if (dStr[i + 1] == 0) {
                    return dStr;
                }

                if (isxdigit(dStr[i + 1]) && isxdigit(dStr[i + 2])) {
                    d = 0;

                    /* combine the next to numbers into one */
                    eStr[0] = dStr[i + 1];
                    eStr[1] = dStr[i + 2];

                    /* convert it to decimal */
                    long int x = strtol(eStr, NULL, 16);

                    /* remove the hex */
                    memmove(&dStr[i + 1], &dStr[i + 3], strlen(&dStr[i + 3]) + 1);

                    dStr[i] = x;
                }
            }
        }
    }

    return dStr;
}

char *generate_index(char *path) {
    FILE *output_f;
    struct dirent **items;
    int total;
    int i = 0;

    char *temp_file = "/tmp/nanoweb_XXXXXX.html";
    /*char temp_file[] = "/tmp/nanoweb_XXXXXX.html";*/
    /*int temp_fd = mkstemps(temp_file, 4);*/
    int temp_fd = mkstemp(temp_file);
    if (temp_fd < 1) {
        log_message(LOG, "Error creating temp file for index", temp_file, errno);
    }
    log_message(LOG, "temp file", temp_file, 0);
    close(temp_fd);
    output_f = fopen(temp_file, "w+");

    fprintf(output_f, "<html><head><style>.contain{max-width:800px;margin:0 auto;border:1px solid #ddd;border-radius:5px;padding:20px;} .hd{font-weight:bold;font-size:16px;}</style>\n");
    fprintf(output_f, "</head><body><div class=\"contain\"><div class=\"hd\">Directory listing for %s</div>\n<ol>\n", path);

    total = scandir(path, &items, NULL, alphasort);
    if (total > 0) {
        while (i < total) {
            if (items[i]->d_name[0] == '.') {
                free(items[i]);
                ++i;
                continue;
            }
            fprintf(output_f, "<li><a href=\"/%s/%s\">%s</a></li>\n", path, items[i]->d_name, items[i]->d_name);
            free(items[i]);
            ++i;
        }
        free(items);
    }

    fprintf(output_f, "</ol></div></body></html>");
    fclose(output_f);

    return temp_file;
    char *filename = malloc(sizeof(char) * (strlen(temp_file)));
    return filename;
}

/* This is a child web server process, so we can exit on errors */
void web(int fd, int hit) {
    int j, file_fd, buflen;
    long i, ret, len;
    char *fstr;
    static char buffer[BUFSIZE + 1];  /* static so zero filled */

    ret = read(fd, buffer, BUFSIZE);  /* read Web request in one go */
    if (ret == 0 || ret == -1) {  /* read failure stop now */
        log_message(FORBIDDEN, "failed to read browser request", "", fd);
    }

    if (ret > 0 && ret < BUFSIZE) {  /* return code is valid chars */
        buffer[ret] = 0;  /* terminate the buffer */
    } else {
        buffer[0] = 0;
    }

    /* Remove CF and LF characters */
    for (i = 0; i < ret; i++) {
        if (buffer[i] == '\r' || buffer[i] == '\n') {
            buffer[i] = '*';
        }
    }

    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
        log_message(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    }

    for (i = 4; i < BUFSIZE; i++) {  /* null terminate after the second space to ignore extra stuff */
        if (buffer[i] == ' ') {  /* string is "GET URL " +lots of other stuff */
            buffer[i] = 0;
            break;
        }
    }

    log_message(LOG, "request", buffer, hit);

    /* Check for illegal parent directory use .. */
    for (j = 0; j < i - 1; j++) {
        if (buffer[j] == '.' && buffer[j + 1] == '.') {
            log_message(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
        }
    }

    /* Convert no filename to index file */
    if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) {
        (void)strcpy(buffer, "GET  ./");
    }

    char *path_uri = url_decode(&buffer[5]);

    /* Check if requested resource is a directory */
    struct stat sb;
    if (stat(path_uri, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        log_message(LOG, "Is a directory", path_uri, fd);
        buflen = strlen(path_uri);
        char dir_path[buflen + 1];
        strcpy(dir_path, path_uri); // Save the original path requested
        if (path_uri[buflen - 1] == '/') {
            (void)strcat(path_uri, "index.html");
            dir_path[strlen(dir_path) - 1] = 0; // Strip off ending slash
        } else {
            (void)strcat(path_uri, "/index.html");
        }

        // Attempt to open index file
        if ((file_fd = open(path_uri, O_RDONLY)) == -1) {
            log_message(LOG, "No index file in dir; generating index", dir_path, fd);
            char *index_name = generate_index(dir_path);
            (void)strcpy(path_uri, index_name);
            /*free(index_name);*/
        }
    }

    /* Work out the file type and check we support it */
    buflen = strlen(path_uri);
    fstr = (char *)0;
    for (i = 0; extensions[i].ext != 0; i++) {
        len = strlen(extensions[i].ext);
        log_message(LOG, "buf", &path_uri[buflen - len], 0);
        if (!strncmp(&path_uri[buflen - len], extensions[i].ext, len)) {
            fstr = extensions[i].filetype;
            break;
        }
    }
    if (fstr == 0) {
        log_message(FORBIDDEN, "File extension type not supported", path_uri, fd);
    }

    /* Open the file for reading */
    if ((file_fd = open(path_uri, O_RDONLY)) == -1) {
        log_message(NOTFOUND, "failed to open file", path_uri, fd);
    }

    log_message(LOG, "SEND", path_uri, hit);
    free(path_uri);

    /* lseek to the file end to find the length */
    len = (long)lseek(file_fd, (off_t)0, SEEK_END);

    /* lseek back to the file start ready for reading */
    (void)lseek(file_fd, (off_t)0, SEEK_SET);

    /* Header + a blank line */
    (void)sprintf(buffer, "HTTP/1.1 200 OK\nServer: nanoweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
    (void)write(fd, buffer, strlen(buffer));

    /* Send file in 8KB block - last block may be smaller */
    while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
        (void)write(fd, buffer, ret);
    }

    /* Allow socket to drain before signalling the socket is closed */
    sleep(1);

    close(fd);
    exit(1);
}

int main(int argc, char **argv) {
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

    if (chdir(argv[2]) == -1) {
        (void)printf("ERROR: Cannot change to directory '%s'\n", argv[2]);
        exit(4);
    }

    // Become deamon + unstopable and no zombies children (= no wait())
    if (fork() != 0) {
        return 0; // parent returns OK to shell
    }

    (void)printf("Starting nanoweb server on port %s, root directory '%s', PID %d\n", argv[1], argv[2], getpid());

    (void)signal(SIGCHLD, SIG_IGN); /* ignore child death */
    (void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */

    (void)setpgrp(); /* break away from process group */
    log_message(LOG, "Starting nanoweb server on port ", argv[1], getpid());

    for (i = 0; i < 32; i++) {
        (void)close(i); /* close open files */
    }

    /* Setup the network socket */
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
