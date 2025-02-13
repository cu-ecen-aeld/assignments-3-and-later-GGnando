#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Open syslog for logging
    openlog("finder-app/writer", LOG_PID, LOG_USER);

    // Validate arguments
    if (argc != 3) {
        syslog(LOG_ERR, "Illegal number of parameters - usauge: writer.sh writefile writestr");
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // Open the file for writing
    int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC,
                  S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file: %s", writefile);
        closelog();
        return 1;
    }

    // Write the text to the file
    ssize_t bytes_written = write(fd, writestr, strlen(writestr));
    if (bytes_written == -1) {
        syslog(LOG_ERR, "Failed to write to file: %s", writefile);
        close(fd);
        closelog();
        return 1;
    }

    // Log successful write operation per instructions
    syslog(LOG_DEBUG, "Writing '%s' to '%s'", writestr, writefile);

    // Close file descriptor and syslog
    close(fd);
    closelog();

    return 0;
}
