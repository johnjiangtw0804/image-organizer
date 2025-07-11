#include <filesystem>

// image
#include <exiv2/exiv2.hpp>
#include <exiv2/error.hpp>

// video
#include "MediaInfo/MediaInfo.h"

#include <functional>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <ctime>

#include <locale>
#include <codecvt>

namespace fs = std::filesystem;

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
    #ifdef UNICODE
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.to_bytes(s);
    #else
        return s;
    #endif
}

// Convert std::filesystem::file_time_type to time_t
inline time_t file_time_to_time_t(fs::file_time_type ftime)
{
    auto raw = std::chrono::system_clock::now() + (ftime - decltype(ftime)::clock::now());
    auto time_point = std::chrono::time_point_cast<std::chrono::system_clock::duration>(raw);
    return std::chrono::system_clock::to_time_t(time_point);
}

// Format time_t to "YYYY-MM-DD"
inline std::string time_t_to_date_string(time_t t)
{
    tm *local_tm = localtime(&t);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", local_tm);
    return std::string(buf);
}

inline bool move_to_file(const fs::path &from, fs::path to)
{
    try {
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

int main()
{
    fs::path current_dir = fs::current_path();

    for (const auto &entry : fs::directory_iterator(current_dir))
    {
        std::cout << "Reading: " << entry.path().filename() << std::endl;

        if (!entry.is_regular_file())
        {
            continue;
        }

        fs::path file_path = entry.path();
        std::string extension = file_path.extension().string();
        transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        // ================================
        // Handle image formats using EXIF
        // ================================
        if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".heic")
        {
            try
            {
                std::unique_ptr<Exiv2::Image> image = Exiv2::ImageFactory::open(file_path.string());
                image->readMetadata();
                auto &exif_data = image->exifData();

                auto it = exif_data.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
                std::string date_part;

                if (it == exif_data.end())
                {
                    // Note: this is not the correct picture taken time
                    auto ftime = fs::last_write_time(entry);
                    time_t time_seconds = file_time_to_time_t(ftime);
                    date_part = time_t_to_date_string(time_seconds);
                }
                else
                {
                    std::string extracted_date = it->value().toString();
                    date_part = extracted_date.substr(0, 10);
                    replace(date_part.begin(), date_part.end(), ':', '-');
                }

                fs::path target_dir = current_dir / date_part;
                if (!fs::exists(target_dir))
                {
                    fs::create_directory(target_dir);
                }

                fs::path target_file_path = target_dir / file_path.filename();

                move_to_file(file_path, target_file_path);
            }
            catch (const Exiv2::Error &e)
            {
                std::cerr << "EXIF read failed: " << e.what() << std::endl;
            }
        }

        // ================================
        // Video format handling here
        // ================================
        if (extension == ".mov" || extension == ".mp4") {
            using namespace MediaInfoLib;
            MediaInfo MI;

            MI.Open(make_mi_string(file_path.string()));

            // it depends on different
            String recordedDate = MI.Get(Stream_General, 0, __T("Recorded_Date"));
            String encodedDate = MI.Get(Stream_General, 0, __T("Encoded_Date"));
            String taggedDate = MI.Get(Stream_General, 0, __T("Tagged_Date"));
            String date;
            if (!recordedDate.empty())
            {
                date = recordedDate;
            }
            else if (!encodedDate.empty())
            {
                date = encodedDate;
            }
            else
            {
                date = taggedDate;
            }
            std::string date_part = make_std_string(date).substr(0, 10);
            MI.Close();

            fs::path target_dir =  current_dir / date_part;
            fs::path target_file_path = target_dir / file_path.filename();

            if (!fs::exists(target_dir))
            {
                fs::create_directory(target_dir);
            }

            move_to_file(file_path, target_file_path);
        }
    }

    return 0;
}
