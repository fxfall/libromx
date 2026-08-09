#include <romx/romx.h>

#include <stdio.h>

int main(int argc, char **argv)
{
    romx_writer_options_t options = ROMX_WRITER_OPTIONS_INIT;
    romx_writer_report_t report = ROMX_WRITER_REPORT_INIT;
    romx_error_t error;
    const char *metadata;
    const char *cover;
    romx_result_t result;

    if (argc < 4 || argc > 5) {
        fprintf(stderr,
            "usage: %s OUTPUT.romx PAYLOAD.rom METADATA.json [COVER.png]\n",
            argv[0]);
        return 2;
    }
    metadata = argv[3];
    cover = argc == 5 ? argv[4] : NULL;
    options.flags = ROMX_WRITER_BODY_SHA256;
    result = romx_writer_write_paths(argv[1], argv[2], metadata, cover,
        &options, &report, &error);
    if (result != ROMX_OK) {
        fprintf(stderr, "%s: %s\n", romx_result_string(result), error.message);
        return 1;
    }
    printf("wrote %llu bytes, payload CRC32 %08x\n",
        (unsigned long long)report.file_size,
        (unsigned int)report.payload_crc32);
    return 0;
}
