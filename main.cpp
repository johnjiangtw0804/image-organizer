#include <filesystem>
#include <exiv2/exiv2.hpp>
#include<iostream>
#include <exiv2/error.hpp>

using namespace std;

inline time_t file_time_to_time_t(std::filesystem::file_time_type ftime)
{
    using namespace std::chrono;
    auto sctp = system_clock::now() + (ftime - decltype(ftime)::clock::now());

    // Step 2: duration 可能是 common_type，要強制 cast
    auto tp = time_point_cast<system_clock::duration>(sctp);

    // to seconds
    return system_clock::to_time_t(tp);
}

inline string time_t_to_date_string(time_t t) {
    tm *local_tm = localtime(&t);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", local_tm);
    return std::string(buf);
}

int main() {
    filesystem::path currentDir = filesystem::current_path();

    for (const filesystem::directory_entry &currentEntry : filesystem::directory_iterator(currentDir))
    {
        cout << "reading from " << currentEntry.path().filename() << endl;
        if (currentEntry.is_regular_file())
        {
            filesystem::path filePath = currentEntry.path();
            string extension = filePath.extension().string();
            // C style tolower
            transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

            // ================================
            // 1) 圖片格式 => 用 EXIF
            // ================================
            if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".heic")
            {
                try {
                    unique_ptr<Exiv2::Image> image = Exiv2::ImageFactory::open(filePath.string());

                    // 不先呼叫 readMetadata() → exifData() 裡面會是空的！
                    image -> readMetadata();
                    auto &exifData = image->exifData();

                    Exiv2::ExifData::iterator it = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
                    string datePart = "";
                    if (it == exifData.end()) {
                        std::filesystem::file_time_type ftime = filesystem::last_write_time(currentEntry);
                        time_t timeSeconds = file_time_to_time_t(ftime);
                        datePart = time_t_to_date_string(timeSeconds);
                    } else {
                        string extractedDate = it->value().toString();
                        datePart = extractedDate.substr(0, 10);
                        replace(datePart.begin(), datePart.end(), ':', '-');
                    }

                    filesystem::path targetDir = currentDir / datePart;
                    if (filesystem::exists(targetDir))
                    {
                        cerr << "File already exists: " << targetDir << endl;
                    } else {
                        filesystem::create_directory(targetDir);
                    }

                    filesystem::path targetFile = targetDir / filePath.filename();
                    int counter = 1;
                    while (filesystem::exists(targetFile))
                    {
                        // stem() => file name without extension
                        targetFile = targetDir / (filePath.stem().string() + "_" + to_string(counter) + filePath.extension().string());
                        counter++;
                    }
                    filesystem::rename(filePath, targetFile);
                    cout << "Moved " << filePath << " -> " << targetFile << endl;
                } catch (const Exiv2::Error &e)
                {
                    std::cerr << "EXIF read failed: " << e.what() << std::endl;
                }
            }

            // ================================
            // 1) 影片格式 => 用 EXIF
            // ================================
        }
    }
    return 0;
}

