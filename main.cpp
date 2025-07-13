// std
#include <functional>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <format> // NEW: C++20 日期格式化
#include <optional>
#include <filesystem>
#include <system_error> // for error code

// image
#include <exiv2/exiv2.hpp>
#include <exiv2/error.hpp>

// video
#include "MediaInfo/MediaInfo.h"


namespace fs = std::filesystem;

// inline in C++ is a keyword that asks the compiler to insert the function’s
// code directly at the place where it’s called, instead of doing a regular function call.
inline MediaInfoLib::String make_mi_string(const std::string &s)
{
#ifdef UNICODE
    return std::wstring(s.begin(), s.end());
#else
    return s;
#endif
}

inline std::string make_std_string(const MediaInfoLib::String &s)
{
    std::string result;
    result.reserve(s.size());
    for (wchar_t wc : s)
    {
        result.push_back(static_cast<char>(wc));
    }
    return result;
}

class MediaInfoRAII {
    public:
        MediaInfoLib::MediaInfo MI;
        MediaInfoRAII() = default;
        MediaInfoRAII(const MediaInfoRAII &) = delete;
        MediaInfoRAII& operator= (const MediaInfoRAII &) = delete;

        MediaInfoRAII(const MediaInfoRAII &&) = delete;
        MediaInfoRAII &operator=(const MediaInfoRAII &&) = delete;

        ~MediaInfoRAII()
        {
            MI.Close();
        }
};

inline std::optional<std::string> extract_media_date(const fs::path& path) {
    using namespace MediaInfoLib;
    MediaInfoRAII raii;
    raii.MI.Open(make_mi_string(path.string()));
    // __T is a macro that ensures correct wide/narrow literal
    String recordedDate = raii.MI.Get(Stream_General, 0, __T("Recorded_Date"));
    String encodedDate = raii.MI.Get(Stream_General, 0, __T("Encoded_Date"));
    String taggedDate = raii.MI.Get(Stream_General, 0, __T("Tagged_Date"));
    String date;
    if (!recordedDate.empty()) {
        date = recordedDate;
    } else if (!encodedDate.empty())
    {
        date = encodedDate;
    }
    else if (taggedDate.empty())
    {
        date = taggedDate;
    } else {
        return std::nullopt; // No valid date found
    }

    std::string std_date = make_std_string(date).substr(0, 10);
    if (std_date.size() < 10)
        return std::nullopt;
    return std_date;
}

inline std::optional<std::string> extract_exif_date(const fs::path &path)
{
    try {
        auto image = Exiv2::ImageFactory::open(path.string());
        image->readMetadata();
        auto &exif_data = image->exifData();
        auto it = exif_data.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
        if (it == exif_data.end()) {
            return std::nullopt;
        }
        std::string date_part = it->value().toString().substr(0, 10);
        std::replace(date_part.begin(), date_part.end(), ':', '-');
        return date_part;
    } catch (const Exiv2::Error &e)
    {
        return std::nullopt;
    }
}

inline std::string fallback_file_time_date(const fs::path& path) {
    auto ftime = fs::last_write_time(path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - std::chrono::file_clock::now()+
                std::chrono::system_clock::now());

    std::time_t t_c = std::chrono::system_clock::to_time_t(sctp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t_c), "%Y-%m-%d");
    return oss.str();
}

inline bool move_to_file(const fs::path &from, fs::path to)
{
    try
    {
        int counter = 1;
        // in case we have images with the same name that is currently in the folder
        while (fs::exists(to))
        {
            to = from.parent_path() /
                 (from.stem().string() + "_" + std::to_string(counter) + from.extension().string());
            ++counter;
        }
        fs::rename(from, to);
    }
    catch (fs::filesystem_error &e)
    {
        std::cerr << "Move file failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

int main() {
    fs::path current_dir = fs::current_path();
    for (const fs::directory_entry &entry : fs::directory_iterator(current_dir))
    {
        std::cout << "Reading: " << entry.path().filename() << std::endl;

        std::error_code ec;
        if (!entry.is_regular_file(ec)) continue;

        std::string file_extension = entry.path().extension().string();
        transform(file_extension.begin(), file_extension.end(), file_extension.begin(), ::tolower);

        // ================================
        // Handle image formats using EXIF
        // ================================
        if (file_extension == ".jpg" || file_extension == ".jpeg" || file_extension == ".png" ||
                file_extension == ".heic")
        {

            std::optional<std::string> exif_date = extract_exif_date(entry.path());
            std::string date = exif_date ? *exif_date : fallback_file_time_date(entry.path());

            fs::path target_dir = current_dir / date;
            if (!fs::exists(target_dir))
            {
                fs::create_directory(target_dir);
            }

            fs::path target_file_path = target_dir / entry.path().filename();
            move_to_file(entry.path(), target_file_path);
            std::cout << "Moving " << entry.path() << " -> " << target_file_path << '\n';
        }

        // ================================
        // Video format handling here
        // ================================
        if (file_extension == ".mov" || file_extension == ".mp4")
        {
            std::optional<std::string> media_date = extract_media_date(entry.path());
            std::string date = media_date ? *media_date : fallback_file_time_date(entry.path());

            fs::path target_dir = current_dir / date;
            if (!fs::exists(target_dir))
            {
                fs::create_directory(target_dir);
            }
            fs::path target_file_path = target_dir/ entry.path().filename();
            move_to_file(entry.path(), target_file_path);
            std::cout << "Moving " << entry.path() << " -> " << target_file_path << '\n';
        }
    }
}
