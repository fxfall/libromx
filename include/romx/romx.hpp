#ifndef ROMX_ROMX_HPP
#define ROMX_ROMX_HPP

#include <romx/romx.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace romx {

class error : public std::runtime_error {
public:
    error(romx_result_t result, const romx_error_t &detail)
        : std::runtime_error(detail.message[0] != '\0'
              ? detail.message : romx_result_string(result)),
          result_(result), system_code_(detail.system_code),
          byte_offset_(detail.byte_offset)
    {
    }

    romx_result_t result() const noexcept { return result_; }
    int32_t system_code() const noexcept { return system_code_; }
    uint64_t byte_offset() const noexcept { return byte_offset_; }

private:
    romx_result_t result_;
    int32_t system_code_;
    uint64_t byte_offset_;
};

inline void check(romx_result_t result, const romx_error_t &detail)
{
    if (result != ROMX_OK) {
        throw error(result, detail);
    }
}

class reader {
public:
    explicit reader(const std::string &path,
        const romx_reader_options_t *options = nullptr)
        : handle_(nullptr)
    {
        romx_error_t detail = {};
        romx_result_t result = romx_reader_open_path(
            path.c_str(), options, &handle_, &detail);
        check(result, detail);
    }

    ~reader() { romx_reader_close(handle_); }

    reader(reader &&other) noexcept : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    reader &operator=(reader &&other) noexcept
    {
        if (this != &other) {
            romx_reader_close(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    reader(const reader &) = delete;
    reader &operator=(const reader &) = delete;

    romx_info_t info() const
    {
        romx_info_t value = ROMX_INFO_INIT;
        romx_error_t detail = {};
        check(romx_reader_get_info(handle_, &value, &detail), detail);
        return value;
    }

    romx_validation_report_t validate(
        romx_validate_flags_t flags = ROMX_VALIDATE_ALL) const
    {
        romx_validation_report_t report = ROMX_VALIDATION_REPORT_INIT;
        romx_error_t detail = {};
        check(romx_reader_validate(handle_, flags, &report, &detail), detail);
        return report;
    }

    std::string payload_format() const
    {
        char format[8];
        uint64_t required = 0;
        romx_error_t detail = {};
        check(romx_reader_get_payload_format(handle_, format,
            sizeof(format), &required, &detail), detail);
        return std::string(format);
    }

    void extract_payload(const std::string &destination,
        const romx_extract_options_t *options = nullptr) const
    {
        romx_error_t detail = {};
        check(romx_extract_payload_path(handle_, destination.c_str(),
            options, &detail), detail);
    }

    romx_reader_t *get() const noexcept { return handle_; }

private:
    romx_reader_t *handle_;
};

inline romx_writer_report_t write_paths(const std::string &destination,
    const std::string &payload, const std::string &metadata = std::string(),
    const std::string &cover = std::string(),
    const romx_writer_options_t *options = nullptr)
{
    romx_writer_report_t report = ROMX_WRITER_REPORT_INIT;
    romx_error_t detail = {};
    const char *metadata_path = metadata.empty() ? nullptr : metadata.c_str();
    const char *cover_path = cover.empty() ? nullptr : cover.c_str();
    check(romx_writer_write_paths(destination.c_str(), payload.c_str(),
        metadata_path, cover_path, options, &report, &detail), detail);
    return report;
}

} /* namespace romx */

#endif
