#include <libnotify/notify.h>
#include <libnotify/notification.h>
#include <libnotify/notify-enum-types.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 16
#define RED_MAX 6500
#define RED_MIN 1000
#define NOTIFY_TIMEOUT_MSEC 1000

static char *const command  = "/usr/bin/redshift -v -O %d";
static char *const icon_on  = "redshift-status-on";
static char *const icon_off = "redshift-status-off";

void notifyInit(NotifyNotification **notification)
{
    notify_init("Red");
    *notification = notify_notification_new("Red", NULL, NULL);
    notify_notification_set_timeout(*notification, NOTIFY_TIMEOUT_MSEC);
    notify_notification_set_hint_string(*notification, "x-canonical-private-synchronous", "");
}

void notifyUninit(NotifyNotification **notification)
{
    if (notification == NULL)
        return;
    g_object_unref(G_OBJECT(*notification));
    *notification = NULL;
    notify_uninit();
}

int notify(NotifyNotification **notification, int red, int min, int max)
{
    if (*notification == NULL)
        notifyInit(notification);

    notify_notification_update(*notification, "Red", NULL, red == max ? icon_off : icon_on);
    notify_notification_set_hint_int32(*notification, "value", 100 * (red - min) / (max - min));

    GError *error = NULL;
    notify_notification_show(*notification, &error);
    if (error) {
        g_printerr("ERROR: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    return 0;
}

int getRed(char const *redData, int red, int min, int max)
{
    int num = red + atoi(redData);

    if (num > max)
        return max;
    else if (num < min)
        return min;
    else
        return num;
}

void die(char const *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

void sendMessage(int sock, char const *msg)
{
    int n = write(sock, msg, strlen(msg));
    if (n < 0)
        die("Cannot write to socket");
}

void processMessages(int sock)
{
    const int sz = strlen(command) + BUFFER_SIZE;
    struct sockaddr_in cli_addr;
    char buffer[sz];
    char arg[BUFFER_SIZE];
    int red = RED_MAX;
    NotifyNotification *notification = NULL;
    while(1) {
        socklen_t clilen = sizeof(cli_addr);
        int newsock = accept(sock, (struct sockaddr *)&cli_addr, &clilen);
        if (newsock < 0)
            die("Cannot accept connection");

        int n = read(newsock, arg, BUFFER_SIZE);
        if (n < 0)
            die("Cannot read from socket");
        else if (n == 0)
            break;

        arg[n] = '\0';
        int num = getRed(arg, red, RED_MIN, RED_MAX);
        if (num != red && snprintf(buffer, sz, command, num) > 0) {
            red = num;
            system(buffer);
            // FIXME: Sometimes gives error:
            //        GDBus.Error:org.freedesktop.DBus.Error.ServiceUnknown: The name :1.167 was not provided by any .service files
            if (notify(&notification, red, RED_MIN, RED_MAX) == -1) {
                notifyUninit(&notification);
                notifyInit(&notification);
                notify(&notification, red, RED_MIN, RED_MAX);
            }
        }
        close(newsock);
    };

    notifyUninit(&notification);
}

void help(char const *cmd)
{
    printf("Usage: %s           Runs server (before next command can be used).\n", cmd);
    printf("       %s AMOUNT    Makes the screen redder or less red by AMOUNT.\n", cmd);
    exit(0);
}

int initClient()
{
    int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock < 0)
        die("Cannot open socket");

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_LOCAL;
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (connect(sock,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        die("Cannot connect to server");


    return sock;
}

int initServer()
{
    int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock < 0)
        die("Cannot open socket");

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_LOCAL;

    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        die("Cannot bind socket");

    listen(sock, 2);

    return sock;
}

int main(int argc, const char *argv[])
{
    int sock;
    pid_t pid = 0;

    if (argc == 2) {
        if (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)
            help(argv[0]);
        sock = initClient();
        sendMessage(sock, argv[1]);
    } else {
        sock = initServer();
        pid_t pid = fork();
        if (pid == 0)
            processMessages(sock);
    }

    close(sock);

    return pid >= 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

