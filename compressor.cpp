/*
 * Copyright 2018-2025 Alnicke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 // === FAQ === documentation is available at https://github.com/JustasMasiulis/filepacker
 // * According to License and copyright, you have to give credits when you use a piece of code from the code, repo.
 // * The compression rates can change depending on your file.
 // * The compressed winrar or 7z files wont be effective, so repacking isnt a solution.
 // * Fully usage has given on the program.
 // * big regards, Alnicke.

// include header
#include "compressor.h"


// valid usages, examples.

void usage(const std::string& prog) {
    std::cerr << "  Usage:\n"
        << "  " << " \n"
        << "  " << "Example folder compression : " << prog << " c my_folder compresssed_folder [optional-key]\n"
        << "  " << "Example folder decompression : " << prog << " d compresssed_folder decompressed_folder [optional-key]\n"
        << "  " << "Example file compression : " << prog << " c hello.exe compressed_hello.bin [optional-key]\n"
        << "  " << "Example file decompression : " << prog << " d compressed_hello.bin decompressed_hello.exe [optional-key]\n"
        << "  " << " \n"
        << "  " << prog << " c <input> <output> (-folder|-file) [key]\n"
        << "    (Compress and encrypt. Use -folder for folder archives, -file for file archives.)\n"
        << "  " << prog << " d <input> <output> (-folder|-file) [key]\n"
        << "    (Decrypt and decompress. Use -folder for folder archives, -file for file archives.)\n"
        << "  " << " \n"
        << "    -----------------------------------------------------------------------------------\n"
        << "    Made by : Alnicke\n"
        << "    Shared : thanks to zlib, lzss, xor.\n";
}

// ep
int main(int argc, char* argv[]) {
    if (argc < 5) {
        usage(argv[0]);
        return 1; // finish cuz bad arg.
    }
    std::string mode = argv[1]; // Compress or decompress modes.
    std::string input_path = argv[2];
    std::string output_path = argv[3];
    std::string archive_Type = argv[4];
    bool force_folder = false, force_file = false;
    if (archive_Type == "-folder") // folder archive or??
        force_folder = true;
    else if (archive_Type == "-file") // file archive.
        force_file = true;
    else {
        usage(argv[0]); // example usages.
        return 1; // finish.
    }
    std::vector<uint8_t> key;
    if (argc >= 6) {
        std::string key_string = argv[5]; // optional key.
        if (key_string.empty()) // if its empty generate a new one with size : 16.
            key = generate_key(16);
        else
            key.assign(key_string.begin(), key_string.end());
    }
    else {
        key = generate_key(16); // key generation, size : 16
    }
    try {
        if (mode == "c") {
            size_t input_size = 0;
            std::vector<uint8_t> archive;
            if (force_folder || (fs::is_directory(input_path) && !force_file)) { // check if its a folder. [ref 1]
                std::cout << "Compressing folder: " << input_path << "\n";
                archive = BuildFolderArchive(fs::path(input_path), key); // construction.
                input_size = 0;
                for (auto& p : fs::recursive_directory_iterator(input_path)) {
                    if (fs::is_regular_file(p))
                        input_size += fs::file_size(p);
                }
            }
            else { // else its a file, so proceed on file compression. [look at 'ref 1']
                std::cout << "Compressing file: " << input_path << "\n";
                std::vector<uint8_t> file_data = Read(input_path); // read the file.
                input_size = file_data.size(); // get the size.
                archive = BuildFileArchive(file_data, key); // construct the file archive, compression.
            }
            Write(output_path, archive); // write to disk.
            double compression_rate = (input_size > 0) ? (100.0 * (1.0 - (double)archive.size() / input_size)) : 0.0; // calculate compression rate.
            std::cout << "Archive size: " << archive.size() << " bytes, "
                << "Original size: " << input_size << " bytes, "
                << "Compression rate: " << compression_rate << "%\n";
            for (int i = 0; i <= 100; i++) {
                Progress(i, 100); // progress indicator, cool shit but useless.
            }
            std::cout << "\nCompression complete.\n";
        }
        else if (mode == "d") {
            if (force_folder) {
                std::cout << "Extracting folder archive...\n";
                fs::path out_folder(output_path); // making the output folder, preparing it.
                fs::create_directories(out_folder); // prepared.
                ExtractFolderArchive(input_path, out_folder); // extract and store the extracted on new folder.
                std::cout << "Folder extraction complete to " << output_path << "\n";
            }
            else if (force_file) {
                std::cout << "Extracting file archive...\n";
                std::vector<uint8_t> file_data = ExtractFileArchive(input_path, key); // extract the file.
                Write(output_path, file_data); // write it to disk.
                std::cout << "File extraction complete to " << output_path << "\n";
            }
            else {
                std::vector<uint8_t> archive = Read(input_path); // read the archive
                std::vector<uint8_t> xheaderKey;
                bool folder_status = false;
                {
                    std::vector<uint8_t> temp = archive;
                    Readarchiveheader(temp, xheaderKey, folder_status); // get the enckey from header.
                }
                if (folder_status) {
                    // extracting the resolved data(s) to folder.
                    std::cout << "Extracting folder archive...\n";
                    fs::path outfolder(output_path);
                    fs::create_directories(outfolder);
                    ExtractFolderArchive(input_path, outfolder);
                    std::cout << "Folder extraction complete to " << output_path << "\n";
                }
                else {
                    // extracting the resolved data to file.
                    std::cout << "Extracting file archive...\n";
                    std::vector<uint8_t> file_data = ExtractFileArchive(input_path, key);
                    Write(output_path, file_data);
                    std::cout << "File extraction complete to " << output_path << "\n";
                }
            }
            for (int i = 0; i <= 100; i++) {
                Progress(i, 100);
            }
            std::cout << "\nDecompression complete.\n";
        }
        else {
            usage(argv[0]); // show the usage, cuz bad decision/args.
            return 1; // finish the program.
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "\nError: " << ex.what() << "\n"; // catch the exceptions.
        return 1; // finish the program.
    }
    return 1; // finish the program.
}

