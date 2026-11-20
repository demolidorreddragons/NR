#include "release_manifest.h"
#include "sha256.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define EXPECTED_TITLE_ID "PNEU00001"
#define EXPECTED_CONTENT_ID "IV0000-PNEU00001_00-PS4NEURALENG0000"
#define EXPECTED_PRX_FILE "ps4_neural_engine.prx"

static void set_error(char* output, size_t capacity, const char* format, ...) {
    if (!output || capacity == 0) return;
    va_list args;
    va_start(args, format);
    vsnprintf(output, capacity, format, args);
    va_end(args);
}

static char* trim(char* value) {
    while (*value && isspace((unsigned char)*value)) value++;
    size_t length = strlen(value);
    while (length && isspace((unsigned char)value[length - 1]))
        value[--length] = '\0';
    return value;
}

static int all_digits(const char* value, size_t minimum_length) {
    size_t length = strlen(value);
    if (length < minimum_length) return 0;
    for (size_t i = 0; i < length; ++i)
        if (!isdigit((unsigned char)value[i])) return 0;
    return 1;
}

static int valid_app_version(const char* value) {
    return strlen(value) == 5 &&
           isdigit((unsigned char)value[0]) &&
           isdigit((unsigned char)value[1]) &&
           value[2] == '.' &&
           isdigit((unsigned char)value[3]) &&
           isdigit((unsigned char)value[4]);
}

static int valid_sha256(const char* value) {
    if (strlen(value) != 64) return 0;
    for (size_t i = 0; i < 64; ++i)
        if (!isxdigit((unsigned char)value[i])) return 0;
    return 1;
}

static void lowercase_hex(char* value) {
    for (; *value; ++value) *value = (char)tolower((unsigned char)*value);
}

int pne_validate_release_manifest(const char* manifest_path,
                                  const char* prx_path,
                                  char* error,
                                  size_t error_size) {
    if (!manifest_path || !prx_path) {
        set_error(error, error_size, "invalid release manifest arguments");
        return 0;
    }

    FILE* file = fopen(manifest_path, "rb");
    if (!file) {
        set_error(error, error_size, "cannot open release manifest (errno=%d)", errno);
        return 0;
    }

    int schema_seen = 0, release_seen = 0, installer_seen = 0;
    int app_version_seen = 0, title_seen = 0, content_seen = 0;
    int prx_file_seen = 0, hash_seen = 0, abi_seen = 0, hardware_seen = 0;
    char expected_hash[65] = {0};
    char line[256];
    unsigned line_number = 0;
    int ok = 1;

    while (fgets(line, sizeof(line), file)) {
        line_number++;
        size_t line_length = strlen(line);
        if (line_length == sizeof(line) - 1 &&
            line[line_length - 1] != '\n' && !feof(file)) {
            set_error(error, error_size, "manifest line %u is too long", line_number);
            ok = 0;
            break;
        }

        char* text = trim(line);
        if (!*text || *text == '#') continue;
        char* separator = strchr(text, '=');
        if (!separator) {
            set_error(error, error_size, "manifest line %u has no '='", line_number);
            ok = 0;
            break;
        }
        *separator = '\0';
        char* key = trim(text);
        char* value = trim(separator + 1);
        if (!*key || !*value) {
            set_error(error, error_size, "manifest line %u has an empty key or value", line_number);
            ok = 0;
            break;
        }

#define REQUIRE_ONCE(flag, label) do { \
    if (flag) { set_error(error, error_size, "duplicate manifest key: %s", label); ok = 0; } \
    else flag = 1; \
} while (0)

        if (strcmp(key, "schema") == 0) {
            REQUIRE_ONCE(schema_seen, key);
            if (ok && strcmp(value, "1") != 0) {
                set_error(error, error_size, "unsupported manifest schema");
                ok = 0;
            }
        } else if (strcmp(key, "release_number") == 0) {
            REQUIRE_ONCE(release_seen, key);
            if (ok && !all_digits(value, 3)) {
                set_error(error, error_size, "invalid release_number");
                ok = 0;
            }
        } else if (strcmp(key, "installer_version") == 0) {
            REQUIRE_ONCE(installer_seen, key);
            if (ok && !all_digits(value, 1)) {
                set_error(error, error_size, "invalid installer_version");
                ok = 0;
            }
        } else if (strcmp(key, "app_version") == 0) {
            REQUIRE_ONCE(app_version_seen, key);
            if (ok && !valid_app_version(value)) {
                set_error(error, error_size, "invalid app_version");
                ok = 0;
            }
        } else if (strcmp(key, "title_id") == 0) {
            REQUIRE_ONCE(title_seen, key);
            if (ok && strcmp(value, EXPECTED_TITLE_ID) != 0) {
                set_error(error, error_size, "manifest TITLE_ID mismatch");
                ok = 0;
            }
        } else if (strcmp(key, "content_id") == 0) {
            REQUIRE_ONCE(content_seen, key);
            if (ok && strcmp(value, EXPECTED_CONTENT_ID) != 0) {
                set_error(error, error_size, "manifest CONTENT_ID mismatch");
                ok = 0;
            }
        } else if (strcmp(key, "prx_file") == 0) {
            REQUIRE_ONCE(prx_file_seen, key);
            if (ok && strcmp(value, EXPECTED_PRX_FILE) != 0) {
                set_error(error, error_size, "manifest PRX filename mismatch");
                ok = 0;
            }
        } else if (strcmp(key, "prx_sha256") == 0) {
            REQUIRE_ONCE(hash_seen, key);
            if (ok && !valid_sha256(value)) {
                set_error(error, error_size, "manifest PRX SHA-256 is malformed");
                ok = 0;
            }
            if (ok) {
                memcpy(expected_hash, value, 65);
                lowercase_hex(expected_hash);
            }
        } else if (strcmp(key, "abi_status") == 0) {
            REQUIRE_ONCE(abi_seen, key);
            if (ok && strcmp(value, "unverified") != 0 && strcmp(value, "verified") != 0) {
                set_error(error, error_size, "invalid abi_status");
                ok = 0;
            }
        } else if (strcmp(key, "hardware_status") == 0) {
            REQUIRE_ONCE(hardware_seen, key);
            if (ok && strcmp(value, "not_tested") != 0 && strcmp(value, "tested") != 0) {
                set_error(error, error_size, "invalid hardware_status");
                ok = 0;
            }
        }
#undef REQUIRE_ONCE
        if (!ok) break;
    }

    if (ferror(file) && ok) {
        set_error(error, error_size, "cannot read release manifest (errno=%d)", errno);
        ok = 0;
    }
    if (fclose(file) != 0 && ok) {
        set_error(error, error_size, "cannot close release manifest (errno=%d)", errno);
        ok = 0;
    }
    if (!ok) return 0;

    if (!schema_seen || !release_seen || !installer_seen || !app_version_seen ||
        !title_seen || !content_seen || !prx_file_seen || !hash_seen ||
        !abi_seen || !hardware_seen) {
        set_error(error, error_size, "release manifest is missing a required field");
        return 0;
    }

    char actual_hash[65];
    if (!pne_sha256_file(prx_path, actual_hash)) {
        set_error(error, error_size, "cannot hash bundled PRX (errno=%d)", errno);
        return 0;
    }
    if (strcmp(actual_hash, expected_hash) != 0) {
        set_error(error, error_size, "bundled PRX SHA-256 does not match release manifest");
        return 0;
    }

    if (error && error_size) error[0] = '\0';
    return 1;
}
