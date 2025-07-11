# Intro

This is a simple CLI tool that organizes iPhone images and videos by the date they were taken.

- **Images:** Uses EXIF `DateTimeOriginal` if available.
- **Videos:** Uses MediaInfo `Recorded_Date`, `Encoded_Date`, or `Tagged_Date`.
- **Output:** Organizes files into folders named `YYYY-MM-DD`. These folders are ignored by Git.
