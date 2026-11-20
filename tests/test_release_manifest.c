#define _POSIX_C_SOURCE 200809L
#include "../installer/release_manifest.h"
#include "../installer/sha256.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char valid_hash[] =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

static const char valid_manifest[] =
    "schema=1\n"
    "release_number=006\n"
    "installer_version=1\n"
    "app_version=01.02\n"
    "title_id=PNEU00001\n"
    "content_id=IV0000-PNEU00001_00-PS4NEURALENG0000\n"
    "prx_file=ps4_neural_engine.prx\n"
    "prx_sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
    "abi_status=unverified\n"
    "hardware_status=not_tested\n";

static void write_text(const char* path, const char* value) {
    FILE* file = fopen(path, "wb");
    assert(file != NULL);
    size_t length = strlen(value);
    assert(fwrite(value, 1, length, file) == length);
    assert(fclose(file) == 0);
}

static void test_sha256_known_vector(void) {
    PNE_SHA256_CTX context;
    uint8_t digest[32];
    char hex[65];

    pne_sha256_init(&context);
    pne_sha256_final(&context, digest);
    pne_sha256_hex(digest, hex);
    assert(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);

    pne_sha256_init(&context);
    pne_sha256_update(&context, "a", 1);
    pne_sha256_update(&context, "bc", 2);
    pne_sha256_final(&context, digest);
    pne_sha256_hex(digest, hex);
    assert(strcmp(hex, valid_hash) == 0);

    const char* boundary =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    pne_sha256_init(&context);
    pne_sha256_update(&context, boundary, strlen(boundary));
    pne_sha256_final(&context, digest);
    pne_sha256_hex(digest, hex);
    assert(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);

    char block[1000];
    memset(block, 'a', sizeof(block));
    pne_sha256_init(&context);
    for (unsigned i = 0; i < 1000; ++i)
        pne_sha256_update(&context, block, sizeof(block));
    pne_sha256_final(&context, digest);
    pne_sha256_hex(digest, hex);
    assert(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
}

int main(void) {
    char directory[] = "/tmp/pne-release-test-XXXXXX";
    assert(mkdtemp(directory) != NULL);
    char manifest_path[256], prx_path[256], error[256];
    assert(snprintf(manifest_path, sizeof(manifest_path), "%s/pne-release.ini", directory) > 0);
    assert(snprintf(prx_path, sizeof(prx_path), "%s/ps4_neural_engine.prx", directory) > 0);

    test_sha256_known_vector();
    write_text(prx_path, "abc");
    write_text(manifest_path, valid_manifest);
    assert(pne_validate_release_manifest(manifest_path, prx_path, error, sizeof(error)) == 1);
    assert(error[0] == '\0');

    const char mismatch[] =
        "schema=1\nrelease_number=006\ninstaller_version=1\napp_version=01.02\n"
        "title_id=PNEU00001\ncontent_id=IV0000-PNEU00001_00-PS4NEURALENG0000\n"
        "prx_file=ps4_neural_engine.prx\n"
        "prx_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
        "abi_status=unverified\nhardware_status=not_tested\n";
    write_text(manifest_path, mismatch);
    assert(pne_validate_release_manifest(manifest_path, prx_path, error, sizeof(error)) == 0);
    assert(strstr(error, "SHA-256") != NULL);

    const char wrong_content[] =
        "schema=1\nrelease_number=006\ninstaller_version=1\napp_version=01.02\n"
        "title_id=PNEU00001\ncontent_id=IV0000-WRONG_00-PS4NEURALENG0000\n"
        "prx_file=ps4_neural_engine.prx\nprx_sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
        "abi_status=unverified\nhardware_status=not_tested\n";
    write_text(manifest_path, wrong_content);
    assert(pne_validate_release_manifest(manifest_path, prx_path, error, sizeof(error)) == 0);
    assert(strstr(error, "CONTENT_ID") != NULL);

    const char duplicate_hash[] =
        "schema=1\nrelease_number=006\ninstaller_version=1\napp_version=01.02\n"
        "title_id=PNEU00001\ncontent_id=IV0000-PNEU00001_00-PS4NEURALENG0000\n"
        "prx_file=ps4_neural_engine.prx\nprx_sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
        "prx_sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
        "abi_status=unverified\nhardware_status=not_tested\n";
    write_text(manifest_path, duplicate_hash);
    assert(pne_validate_release_manifest(manifest_path, prx_path, error, sizeof(error)) == 0);
    assert(strstr(error, "duplicate") != NULL);

    const char missing_field[] =
        "schema=1\nrelease_number=006\ninstaller_version=1\napp_version=01.02\n"
        "title_id=PNEU00001\ncontent_id=IV0000-PNEU00001_00-PS4NEURALENG0000\n"
        "prx_file=ps4_neural_engine.prx\nprx_sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
        "abi_status=unverified\n";
    write_text(manifest_path, missing_field);
    assert(pne_validate_release_manifest(manifest_path, prx_path, error, sizeof(error)) == 0);
    assert(strstr(error, "missing") != NULL);

    const char malformed_hash[] =
        "schema=1\nrelease_number=006\ninstaller_version=1\napp_version=01.02\n"
        "title_id=PNEU00001\ncontent_id=IV0000-PNEU00001_00-PS4NEURALENG0000\n"
        "prx_file=ps4_neural_engine.prx\nprx_sha256=not-a-sha256\n"
        "abi_status=unverified\nhardware_status=not_tested\n";
    write_text(manifest_path, malformed_hash);
    assert(pne_validate_release_manifest(manifest_path, prx_path, error, sizeof(error)) == 0);
    assert(strstr(error, "malformed") != NULL);

    const char unknown_key[] =
        "schema=1\nrelease_number=006\ninstaller_version=1\napp_version=01.02\n"
        "title_id=PNEU00001\ncontent_id=IV0000-PNEU00001_00-PS4NEURALENG0000\n"
        "prx_file=ps4_neural_engine.prx\nprx_sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
        "abi_status=unverified\nhardware_status=not_tested\n"
        "prx_shaa256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";
    write_text(manifest_path, unknown_key);
    assert(pne_validate_release_manifest(manifest_path, prx_path, error, sizeof(error)) == 0);
    assert(strstr(error, "unknown manifest key") != NULL);

    unlink(manifest_path);
    unlink(prx_path);
    rmdir(directory);
    puts("Release manifest and SHA-256 integrity tests passed");
    return 0;
}
