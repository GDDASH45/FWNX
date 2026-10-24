
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CONFIGS 1024
#define MAX_NAME 128
#define MAX_PROMPT 512
#define MAX_PATH 1024
#define MAX_DEPTH 32
#define MAX_LINE 2048

struct config {
    char name[MAX_NAME];
    char prompt[MAX_PROMPT];
    char default_value;
    char value;
    int has_default;
    int has_value;
};

static struct config configs[MAX_CONFIGS];
static size_t config_count;

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
        text++;

    if (*text == '\0')
        return text;

    end = text + strlen(text) - 1;

    while (end > text && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';

    return text;
}

static void copy_string(char *dest, size_t size, const char *src)
{
    if (size == 0)
        return;

    snprintf(dest, size, "%s", src);
}

static int find_config(const char *name)
{
    size_t i;

    for (i = 0; i < config_count; i++) {
        if (strcmp(configs[i].name, name) == 0)
            return (int)i;
    }

    return -1;
}

static int add_config(const char *name)
{
    struct config *cfg;
    size_t i;

    i = (size_t)(find_config(name));

    if (find_config(name) >= 0)
        return (int)i;

    if (config_count >= MAX_CONFIGS) {
        fprintf(stderr, "conf: too many configuration symbols\n");
        return -1;
    }

    if (strlen(name) >= MAX_NAME) {
        fprintf(stderr, "conf: configuration name is too long: %s\n",
                name);
        return -1;
    }

    cfg = &configs[config_count];
    memset(cfg, 0, sizeof(*cfg));

    copy_string(cfg->name, sizeof(cfg->name), name);
    copy_string(cfg->prompt, sizeof(cfg->prompt), name);

    cfg->default_value = 'n';
    cfg->value = 'n';

    return (int)config_count++;
}

static int parse_kconfig(const char *path, int depth)
{
    FILE *file;
    char line[MAX_LINE];
    char source_path[MAX_PATH];
    char *text;
    char *current_name = NULL;
    int current_index = -1;
    unsigned long line_number = 0;

    if (depth >= MAX_DEPTH) {
        fprintf(stderr, "conf: maximum source depth exceeded: %s\n",
                path);
        return -1;
    }

    file = fopen(path, "r");

    if (!file) {
        fprintf(stderr, "conf: cannot open %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *value;
        line_number++;

        text = trim(line);

        if (*text == '\0' || *text == '#')
            continue;

        if (strncmp(text, "mainmenu", 8) == 0 &&
            isspace((unsigned char)text[8])) {
            continue;
        }

        if (strncmp(text, "config", 6) == 0 &&
            isspace((unsigned char)text[6])) {
            char name[MAX_NAME];
            size_t i = 0;

            value = trim(text + 6);

            while (*value && !isspace((unsigned char)*value)) {
                if (i + 1 >= sizeof(name)) {
                    fprintf(stderr,
                            "conf: symbol name too long in %s:%lu\n",
                            path, line_number);
                    fclose(file);
                    return -1;
                }

                name[i++] = *value++;
            }

            name[i] = '\0';

            if (i == 0) {
                fprintf(stderr,
                        "conf: missing symbol name in %s:%lu\n",
                        path, line_number);
                fclose(file);
                return -1;
            }

            current_index = add_config(name);

            if (current_index < 0) {
                fclose(file);
                return -1;
            }

            current_name = configs[current_index].name;
            continue;
        }

        if (strncmp(text, "source", 6) == 0 &&
            isspace((unsigned char)text[6])) {
            char *start;
            char *end;
            size_t length;

            value = trim(text + 6);
            start = strchr(value, '"');

            if (!start) {
                fprintf(stderr,
                        "conf: invalid source directive in %s:%lu\n",
                        path, line_number);
                fclose(file);
                return -1;
            }

            start++;
            end = strchr(start, '"');

            if (!end) {
                fprintf(stderr,
                        "conf: unterminated source path in %s:%lu\n",
                        path, line_number);
                fclose(file);
                return -1;
            }

            length = (size_t)(end - start);

            if (length == 0 || length >= sizeof(source_path)) {
                fprintf(stderr,
                        "conf: invalid source path in %s:%lu\n",
                        path, line_number);
                fclose(file);
                return -1;
            }

            memcpy(source_path, start, length);
            source_path[length] = '\0';

            if (parse_kconfig(source_path, depth + 1) != 0) {
                fclose(file);
                return -1;
            }

            continue;
        }

        if (current_index < 0 || !current_name)
            continue;

        if (strncmp(text, "bool", 4) == 0 &&
            (text[4] == '\0' ||
             isspace((unsigned char)text[4]))) {
            char *start;
            char *end;
            size_t length;

            value = trim(text + 4);
            start = strchr(value, '"');

            if (start) {
                start++;
                end = strchr(start, '"');

                if (end) {
                    length = (size_t)(end - start);

                    if (length >= sizeof(configs[current_index].prompt))
                        length = sizeof(configs[current_index].prompt) - 1;

                    memcpy(configs[current_index].prompt, start, length);
                    configs[current_index].prompt[length] = '\0';
                }
            }

            continue;
        }

        if (strncmp(text, "default", 7) == 0 &&
            isspace((unsigned char)text[7])) {
            value = trim(text + 7);

            if (value[0] == 'y' &&
                (value[1] == '\0' ||
                 isspace((unsigned char)value[1]))) {
                configs[current_index].default_value = 'y';
                configs[current_index].has_default = 1;
            } else if (value[0] == 'n' &&
                       (value[1] == '\0' ||
                        isspace((unsigned char)value[1]))) {
                configs[current_index].default_value = 'n';
                configs[current_index].has_default = 1;
            }

            continue;
        }
    }

    if (ferror(file)) {
        fprintf(stderr, "conf: error reading %s\n", path);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int load_config(const char *path)
{
    FILE *file;
    char line[MAX_LINE];

    file = fopen(path, "r");

    if (!file) {
        if (errno == ENOENT)
            return 0;

        fprintf(stderr, "conf: cannot read %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *text;
        char name[MAX_NAME];
        char *equals;
        char *value;
        size_t length;
        int index;

        text = trim(line);

        if (strncmp(text, "# CONFIG_", 9) == 0) {
            value = text + 9;
            equals = strstr(value, " is not set");

            if (!equals)
                continue;

            *equals = '\0';

            index = find_config(value);

            if (index >= 0) {
                configs[index].value = 'n';
                configs[index].has_value = 1;
            }

            continue;
        }

        if (strncmp(text, "CONFIG_", 7) != 0)
            continue;

        value = text + 7;
        equals = strchr(value, '=');

        if (!equals)
            continue;

        length = (size_t)(equals - value);

        if (length == 0 || length >= sizeof(name))
            continue;

        memcpy(name, value, length);
        name[length] = '\0';

        value = equals + 1;

        if (value[0] != 'y' && value[0] != 'n')
            continue;

        if (value[1] != '\0')
            continue;

        index = find_config(name);

        if (index >= 0) {
            configs[index].value = value[0];
            configs[index].has_value = 1;
        }
    }

    if (ferror(file)) {
        fprintf(stderr, "conf: error reading %s\n", path);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int ask_config(struct config *cfg)
{
    char answer[64];
    char default_value;

    if (cfg->has_value)
        return 0;

    default_value = cfg->has_default
                        ? cfg->default_value
                        : 'n';

    for (;;) {
        printf("%s (%s) [%c/%c] ",
               cfg->prompt,
               cfg->name,
               default_value == 'y' ? 'Y' : 'y',
               default_value == 'n' ? 'N' : 'n');

        fflush(stdout);

        if (!fgets(answer, sizeof(answer), stdin)) {
            cfg->value = default_value;
            cfg->has_value = 1;
            putchar('\n');
            return 0;
        }

        {
            char *text = trim(answer);

            if (*text == '\0') {
                cfg->value = default_value;
                cfg->has_value = 1;
                return 0;
            }

            if (text[0] == 'y' || text[0] == 'Y') {
                cfg->value = 'y';
                cfg->has_value = 1;
                return 0;
            }

            if (text[0] == 'n' || text[0] == 'N') {
                cfg->value = 'n';
                cfg->has_value = 1;
                return 0;
            }
        }

        printf("Please answer y or n.\n");
    }
}

static int write_config(const char *path)
{
    FILE *file;
    size_t i;

    file = fopen(path, "w");

    if (!file) {
        fprintf(stderr, "conf: cannot write %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    fprintf(file, "#\n");
    fprintf(file, "# FWNX Kernel Configuration\n");
    fprintf(file, "# Generated by scripts/kconfig/conf\n");
    fprintf(file, "#\n\n");

    for (i = 0; i < config_count; i++) {
        if (!configs[i].has_value)
            configs[i].value = configs[i].has_default
                                   ? configs[i].default_value
                                   : 'n';

        if (configs[i].value == 'y') {
            fprintf(file, "CONFIG_%s=y\n", configs[i].name);
        } else {
            fprintf(file, "# CONFIG_%s is not set\n",
                    configs[i].name);
        }
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "conf: error closing %s\n", path);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *kconfig_path = "Kconfig";
    const char *config_path = ".config";
    size_t i;

    if (argc > 3) {
        fprintf(stderr, "Usage: %s [Kconfig] [.config]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc >= 2)
        kconfig_path = argv[1];

    if (argc >= 3)
        config_path = argv[2];

    if (parse_kconfig(kconfig_path, 0) != 0)
        return EXIT_FAILURE;

    if (load_config(config_path) != 0)
        return EXIT_FAILURE;

    for (i = 0; i < config_count; i++) {
        if (ask_config(&configs[i]) != 0)
            return EXIT_FAILURE;
    }

    if (write_config(config_path) != 0)
        return EXIT_FAILURE;

    printf("Configuration written to %s\n", config_path);

    return EXIT_SUCCESS;
}
