#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define BASE_RRD_PATH "/opt/collectd/var/lib/collectd/rrd/localhost/"
#define MAX_METRIC_LEN 32
#define MAX_CORES 128
#define MAX_DIR_LEN 256
#define MAX_CMD_LEN 32768
#define MAX_PERIOD_LEN 32

// Палитра цветов для разных ядер
const char *core_colors[] = {
    "#FF0000", "#00FF00", "#0000FF", "#FFFF00", "#FF00FF", "#00FFFF",
    "#800000", "#008000", "#000080", "#808000", "#800080", "#008080",
    "#C0C0C0", "#808080", "#999999", "#FFA500", "#FF4500", "#32CD32",
    "#8A2BE2", "#DC143C", "#00CED1", "#FF8C00", "#7CFC00", "#4682B4"};

const int num_colors = sizeof(core_colors) / sizeof(core_colors[0]);

void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

int get_query_value(const char *query, const char *key, char *value, size_t maxlen)
{
    if (!query)
        return 0;
    size_t keylen = strlen(key);
    const char *p = query;

    while (*p)
    {
        if (strncmp(p, key, keylen) == 0 && p[keylen] == '=')
        {
            p += keylen + 1;
            size_t i = 0;
            while (*p && *p != '&' && i + 1 < maxlen)
            {
                value[i++] = *p++;
            }
            value[i] = '\0';
            char decoded[maxlen];
            url_decode(decoded, value);
            strncpy(value, decoded, maxlen);
            value[maxlen - 1] = '\0';
            return 1;
        }

        while (*p && *p != '&')
            p++;

        if (*p == '&')
            p++;
    }

    return 0;
}

int compare_cores(const void *a, const void *b)
{
    const char *coreA = *(const char **)a;
    const char *coreB = *(const char **)b;

    int numA = atoi(coreA + 4);
    int numB = atoi(coreB + 4);

    return numA - numB;
}

int is_valid_period(const char *period)
{
    if (strncmp(period, "now-", 4) != 0)
    {
        return 0;
    }

    const char *p = period + 4;
    while (*p)
    {
        if (!isdigit(*p))
        {
            char unit = *p;
            if (unit != 's' && unit != 'm' && unit != 'h' && unit != 'd' && unit != 'w' && unit != 'M' && unit != 'y')
            {
                return 0;
            }

            p++;

            if (*p == '\0')
                return 1;

            return 0;
        }
        p++;
    }

    return 0;
}

int main(void)
{
    FILE *error_log = fopen("/var/log/rrd_cgi_errors.log", "a");

    if (error_log)
    {
        dup2(fileno(error_log), STDERR_FILENO);
    }

    char *query = getenv("QUERY_STRING");
    char format[8] = "PNG";
    char metric[MAX_METRIC_LEN] = "cpu-user";
    char period[MAX_PERIOD_LEN] = "now-1h";

    get_query_value(query, "format", format, sizeof(format));
    get_query_value(query, "metric", metric, sizeof(metric));
    get_query_value(query, "period", period, sizeof(period));

    // валидация
    if (strcmp(format, "PNG") != 0 && strcmp(format, "SVG") != 0)
    {
        fprintf(stderr, "Invalid format requested: %s\n", format);
        printf("Status: 400 Bad Request\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Invalid format. Supported formats: PNG, SVG\n");

        if (error_log)
            fclose(error_log);
        return 1;
    }

    if (!is_valid_period(period))
    {
        fprintf(stderr, "Invalid period requested: %s\n", period);
        printf("Status: 400 Bad Request\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Invalid period format. Valid examples: now-1h, now-4h, now-1d\n");
        if (error_log)
            fclose(error_log);

        return 1;
    }

    // список всех ядер
    DIR *dir = opendir(BASE_RRD_PATH);
    if (!dir)
    {
        fprintf(stderr, "Cannot open RRD directory: %s. Error: %s\n", BASE_RRD_PATH, strerror(errno));
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Failed to access RRD data directory\n");
        if (error_log)
            fclose(error_log);

        return 1;
    }

    char *core_dirs[MAX_CORES];
    int core_count = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL && core_count < MAX_CORES)
    {
        if (ent->d_type == DT_DIR &&
            (strncmp(ent->d_name, "cpu-", 4) == 0 ||
             strncmp(ent->d_name, "percent-", 8) == 0))
        {
            // Проверяем что после 'cpu-' идет число
            char *endptr;
            strtol(ent->d_name + 4, &endptr, 10);

            if (*endptr == '\0')
            {
                core_dirs[core_count] = strdup(ent->d_name);
                if (!core_dirs[core_count])
                {
                    fprintf(stderr, "Memory allocation failed\n");
                    continue;
                }
                core_count++;
            }
        }
    }

    closedir(dir);

    if (core_count == 0)
    {
        fprintf(stderr, "No CPU cores found in directory: %s\n", BASE_RRD_PATH);
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("No CPU core data available\n");
        if (error_log)
            fclose(error_log);

        return 1;
    }

    // сортировка ядер по номеру
    qsort(core_dirs, core_count, sizeof(char *), compare_cores);

    // делаем команду для rrdtool
    char cmd[MAX_CMD_LEN];
    int cmd_len = snprintf(
        cmd, sizeof(cmd),
        "/opt/rrdtool/bin/rrdtool graph - "
        "--imgformat %s "
        "--width 800 "
        "--height 400 "
        "--title 'CPU %s Usage by Core (%s)' "
        "--vertical-label '%%' "
        "--lower-limit 0 "
        "--upper-limit 100 "
        "--rigid "
        "--start %s ",
        format, metric, period, period);

    for (int i = 0; i < core_count; i++)
    {
        char rrd_path[512];
        snprintf(rrd_path, sizeof(rrd_path), "%s%s/%s.rrd:value:AVERAGE",
                 BASE_RRD_PATH, core_dirs[i], metric);

        // DEF для данных ядра
        cmd_len += snprintf(
            cmd + cmd_len, sizeof(cmd) - cmd_len,
            "DEF:core%d=%s ", i, rrd_path);

        // LINE с уникальным цветом
        const char *color = core_colors[i % num_colors];
        int core_num = atoi(core_dirs[i] + 4);
        cmd_len += snprintf(
            cmd + cmd_len, sizeof(cmd) - cmd_len,
            "LINE1:core%d%s:'Core %d' ", i, color, core_num);
        free(core_dirs[i]);
    }

    // агрегированный график для всех ядер
    if (core_count > 1)
    {
        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                            "CDEF:total=core0");
        for (int i = 1; i < core_count; i++)
        {
            cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                                ",core%d,ADDNAN", i);
        }
        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
                            " LINE2:total#000000:'Total usage\\l' ");
    }

    // добавляем легенду
    cmd_len += snprintf(
        cmd + cmd_len, sizeof(cmd) - cmd_len,
        "COMMENT:'\\n' "
        "COMMENT:'Generated at \\c' "
        "GPRINT:core0:LAST:'Current\\: %%5.1lf%%%%' "
        "GPRINT:core0:AVERAGE:'Average\\: %%5.1lf%%%%' "
        "GPRINT:core0:MAX:'Maximum\\: %%5.1lf%%%%\\n'");

    printf("Content-Type: image/%s\r\n\r\n",
           strcasecmp(format, "svg") == 0 ? "svg+xml" : "png");
    fflush(stdout);

    // выполняем команду
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
    {
        fprintf(stderr, "Failed to execute rrdtool command. Error: %s\n", strerror(errno));
        fprintf(stderr, "Command: %s\n", cmd);
        if (error_log)
            fclose(error_log);

        return 1;
    }

    // передаем в stdout

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        fwrite(buffer, 1, n, stdout);
    }

    int status = pclose(pipe);
    if (status != 0)
    {
        fprintf(stderr, "rrdtool command failed with exit status %d\n", status);
        fprintf(stderr, "Command: %s\n", cmd);
    }

    if (error_log)
        fclose(error_log);

    return 0;
}