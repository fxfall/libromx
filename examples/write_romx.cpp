#include <romx/romx.hpp>

#include <iostream>

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: " << argv[0]
                  << " OUTPUT.romx PAYLOAD.rom METADATA.json [COVER.png]\n";
        return 2;
    }
    try {
        romx_writer_options_t options = ROMX_WRITER_OPTIONS_INIT;
        options.flags = ROMX_WRITER_BODY_SHA256;
        romx_writer_report_t report = romx::write_paths(
            argv[1], argv[2], argv[3], argc == 5 ? argv[4] : "", &options);
        romx::reader container(argv[1]);
        container.validate();
        std::cout << "wrote " << report.file_size << " bytes\n";
    } catch (const romx::error &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
