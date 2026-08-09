#include <romx/romx.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    const std::string payload = "romx-phase8-cpp.gb";
    const std::string metadata = "romx-phase8-cpp.json";
    const std::string output = "romx-phase8-cpp.romx";
    try {
        {
            std::ofstream stream(payload.c_str(), std::ios::binary);
            stream.write("abc", 3);
        }
        {
            std::ofstream stream(metadata.c_str(), std::ios::binary);
            stream << "{\"schema_version\":\"0.1.0\",\"name\":\"C++\","
                      "\"platform\":\"gb\",\"payload_format\":\"gb\"}";
        }
        romx_writer_options_t options = ROMX_WRITER_OPTIONS_INIT;
        options.flags = ROMX_WRITER_BODY_SHA256;
        romx_writer_report_t written = romx::write_paths(
            output, payload, metadata, std::string(), &options);
        if (written.payload_crc32 != UINT32_C(0x352441c2)) return 1;
        romx::reader first(output);
        romx::reader moved(std::move(first));
        romx_validation_report_t validation = moved.validate();
        if (validation.body_sha256 != ROMX_STATUS_VALID ||
            moved.payload_format() != "gb") return 1;
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    (void)std::remove(output.c_str());
    (void)std::remove(metadata.c_str());
    (void)std::remove(payload.c_str());
    std::cout << "all phase 8 C++ tests passed\n";
    return 0;
}
