#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>
#include <libgen.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_GRAY    "\033[90m"
#define COLOR_LABEL   "\033[38;5;166m"
#define COLOR_GREEN_TEXT "\033[92m"

#define MAX_BUFFER 4096
#define MAX_LINES 50
#define MAX_LINE_LEN 256

typedef struct {
    char dolphin[64];
    char device[32];
    char manufacturer[32];
    char cpu[64];
    char firmware[64];
    char build_date[32];
    char flash[32];
    char sram[32];
    char sd_card[32];
    char serial[32];
    char status[32];
} FlipperInfo;

char** read_logo(const char* filename, int* line_count) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "%s❌ Error: File '%s' not found.%s\n", COLOR_LABEL, filename, COLOR_RESET);
        exit(1);
    }
    char** logo = malloc(MAX_LINES * sizeof(char*));
    char line[MAX_LINE_LEN];
    *line_count = 0;
    while (fgets(line, sizeof(line), file) && *line_count < MAX_LINES) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        logo[*line_count] = malloc(MAX_LINE_LEN);
        strncpy(logo[*line_count], line, MAX_LINE_LEN - 1);
        logo[*line_count][MAX_LINE_LEN - 1] = '\0';
        (*line_count)++;
    }
    fclose(file);
    return logo;
}

void free_logo(char** logo, int line_count) {
    for (int i = 0; i < line_count; i++) free(logo[i]);
    free(logo);
}

int find_flipper_port(char *port, size_t port_size) {
    DIR *dir;
    struct dirent *entry;
    const char *paths[] = {"/dev", "/var/run", NULL};
    for (int i = 0; paths[i] != NULL; i++) {
        dir = opendir(paths[i]);
        if (!dir) continue;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, "cu.usbmodem") || 
                strstr(entry->d_name, "ttyACM") || 
                strstr(entry->d_name, "ttyUSB") || 
                strstr(entry->d_name, "ttyU")) {
                snprintf(port, port_size, "%s/%s", paths[i], entry->d_name);
                closedir(dir);
                return 1;
            }
        }
        closedir(dir);
    }
    return 0;
}

int serial_open(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0) return -1;
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 5;
    tcsetattr(fd, TCSANOW, &options);
    usleep(500000);
    tcflush(fd, TCIOFLUSH);
    return fd;
}

void send_command(int fd, const char *cmd) {
    write(fd, cmd, strlen(cmd));
    write(fd, "\r\n", 2);
    usleep(300000);
}

char* read_response(int fd) {
    static char buffer[MAX_BUFFER];
    memset(buffer, 0, MAX_BUFFER);
    fd_set set;
    struct timeval timeout = {1, 500000};
    FD_ZERO(&set);
    FD_SET(fd, &set);
    if (select(fd + 1, &set, NULL, NULL, &timeout) > 0) {
        int n = read(fd, buffer, MAX_BUFFER - 1);
        if (n > 0) buffer[n] = '\0';
    }
    return buffer;
}

void trim_whitespace(char *str) {
    if (!str) return;
    char *start = str;
    while (*start == ' ' || *start == '\t') start++;
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    *(end + 1) = '\0';
    if (start != str) memmove(str, start, strlen(start) + 1);
}

void parse_info(const char *response, FlipperInfo *info) {
    strcpy(info->device, "Flipper Zero");
    strcpy(info->manufacturer, "Flipper Devices Ltd.");
    strcpy(info->cpu, "STM32WB55");
    strcpy(info->flash, "1 MB");
    strcpy(info->sram, "256 KB");
    strcpy(info->status, "Connected");
    
    strcpy(info->dolphin, "N/A");
    strcpy(info->firmware, "N/A");
    strcpy(info->build_date, "N/A");
    strcpy(info->sd_card, "N/A");
    strcpy(info->serial, "N/A");

    char response_copy[MAX_BUFFER];
    strncpy(response_copy, response, MAX_BUFFER - 1);
    
    char *saveptr;
    char *line = strtok_r(response_copy, "\n", &saveptr);
    while (line != NULL) {
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = line;
            char *value = colon + 1;
            trim_whitespace(key);
            trim_whitespace(value);

            if (strcmp(key, "hardware_name") == 0) {
                strncpy(info->dolphin, value, sizeof(info->dolphin) - 1);
            } else if (strcmp(key, "hardware_uid") == 0) {
                strncpy(info->serial, value, sizeof(info->serial) - 1);
            } else if (strcmp(key, "firmware_version") == 0) {
                strncpy(info->firmware, value, sizeof(info->firmware) - 1);
            } else if (strcmp(key, "firmware_build_date") == 0) {
                strncpy(info->build_date, value, sizeof(info->build_date) - 1);
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

void fetch_info(int fd, FlipperInfo *info) {
    send_command(fd, "device_info");
    char *response = read_response(fd);
    parse_info(response, info);
    
    send_command(fd, "");
    read_response(fd);
    tcflush(fd, TCIOFLUSH);

    send_command(fd, "storage info /ext");
    response = read_response(fd);

    char *total_kb_ptr = strstr(response, "KiB total");
    char *free_kb_ptr = strstr(response, "KiB free");
    long long total_kb = 0;
    long long free_kb = 0;

    // Парсим общее место
    if (total_kb_ptr) {
        char *start = total_kb_ptr;
        while (start > response && *(start-1) != ' ' && *(start-1) != '\n') start--;
        if (start > response) {
            total_kb = atoll(start);
        }
    }

    // Парсим свободное место
    if (free_kb_ptr) {
        char *start = free_kb_ptr;
        while (start > response && *(start-1) != ' ' && *(start-1) != '\n') start--;
        if (start > response) {
            free_kb = atoll(start);
        }
    }

    // Формируем строку: Занято / Всего
    if (total_kb > 0 && free_kb > 0) {
        long long used_kb = total_kb - free_kb;
        double used_gb = (double)used_kb / 1048576.0;
        double total_gb = (double)total_kb / 1048576.0;
        snprintf(info->sd_card, sizeof(info->sd_card), "%.1f GB / %.1f GB", used_gb, total_gb);
    } else if (total_kb > 0) {
        long long total_gb = total_kb / 1048576;
        snprintf(info->sd_card, sizeof(info->sd_card), "%lld GB", total_gb);
    }
}

void display_info(FlipperInfo *info, char** logo, int logo_lines) {
    char* user = getlogin();
    if (!user) user = getenv("USER");
    if (!user) user = "user";

    char hostname[256] = {0};
    gethostname(hostname, sizeof(hostname) - 1);
    char *first_dot = strchr(hostname, '.');
    if (first_dot) *first_dot = '\0';

    int max_logo_width = 0;
    for (int i = 0; i < logo_lines; i++) {
        int len = 0; 
        const char *p = logo[i];
        while (*p) {
            if (*p == '\033') { while(*p && *p != 'm') p++; if(*p) p++; }
            else { len++; p++; }
        }
        if (len > max_logo_width) max_logo_width = len;
    }

    int column_offset = max_logo_width + 4;
    int offset = 2;

    char info_lines[11][256];
    snprintf(info_lines[0], sizeof(info_lines[0]), "%sDolphin:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->dolphin);
    snprintf(info_lines[1], sizeof(info_lines[1]), "%sDevice:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->device);
    snprintf(info_lines[2], sizeof(info_lines[2]), "%sManufacturer:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->manufacturer);
    snprintf(info_lines[3], sizeof(info_lines[3]), "%sCPU:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->cpu);
    snprintf(info_lines[4], sizeof(info_lines[4]), "%sFirmware:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->firmware);
    snprintf(info_lines[5], sizeof(info_lines[5]), "%sBuild Date:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->build_date);
    snprintf(info_lines[6], sizeof(info_lines[6]), "%sFlash:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->flash);
    snprintf(info_lines[7], sizeof(info_lines[7]), "%sSRAM:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->sram);
    snprintf(info_lines[8], sizeof(info_lines[8]), "%sSD Card:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->sd_card);
    snprintf(info_lines[9], sizeof(info_lines[9]), "%sSerial:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_WHITE, info->serial);
    snprintf(info_lines[10], sizeof(info_lines[10]), "%sStatus:%s %s%s", COLOR_LABEL, COLOR_RESET, COLOR_GREEN_TEXT, info->status);

    printf("\n");

    int total_lines = (logo_lines > 11) ? logo_lines : 11;

    for (int i = 0; i < total_lines; i++) {
        if (i < logo_lines) printf("  %s%s", COLOR_LABEL, logo[i]);
        else printf("  ");

        if (i == offset) {
            printf("\033[%dG%s%s@%s%s", column_offset, COLOR_WHITE, user, hostname, COLOR_RESET);
        } else if (i > offset && i < offset + 1 + 11) {
            printf("\033[%dG%s", column_offset, info_lines[i - offset - 1]);
        } else {
            printf("\033[%dG", column_offset);
        }
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    char port[256] = {0};
    int fd = -1;
    FlipperInfo info = {0};
    
    char path[512];
    char *dir = dirname(strdup(argv[0]));
    snprintf(path, sizeof(path), "%s/flogo.txt", dir);

    int logo_lines = 0;
    char** logo = read_logo(path, &logo_lines);
    
    if (!find_flipper_port(port, sizeof(port))) {
        printf("%s❌ Flipper Zero not found. Is it connected?%s\n", COLOR_LABEL, COLOR_RESET);
        free_logo(logo, logo_lines);
        return 1;
    }
    
    fd = serial_open(port);
    if (fd < 0) {
        printf("%s❌ Failed to connect to %s. Check permissions.%s\n", COLOR_LABEL, port, COLOR_RESET);
        printf("%sLinux: sudo usermod -aG dialout $USER && reboot\n", COLOR_WHITE);
        printf("%sFreeBSD: sudo pw groupmod dialer -m $USER && logout/login%s\n", COLOR_WHITE, COLOR_RESET);
        free_logo(logo, logo_lines);
        return 1;
    }
    
    fetch_info(fd, &info);
    display_info(&info, logo, logo_lines);
    
    close(fd);
    free_logo(logo, logo_lines);
    return 0;
}