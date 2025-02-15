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

// includes
#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>
#include <zlib.h>
#include <thread>
// includes

// vars ( compiletime, static ) 
namespace fs = std::filesystem;
constexpr size_t WINDOW = 65535;
constexpr size_t MIN = 3;
constexpr size_t MAX = 130;
constexpr size_t MAGIC_SIZE = 4;
const char MAGIC[MAGIC_SIZE + 1] = "ACOD";
constexpr uint8_t EKEY = 0xF * 91837596798 ^ 901038597 + 9813798756871379;
constexpr uint8_t FOLDER_FLAG = 1;
// vars ( compiletime, static )

std::vector<uint8_t> Read(const std::string& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f)
        throw std::runtime_error("Error opening file: " + file);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
}

void Write(const std::string& file, const std::vector<uint8_t>& data) {
    std::ofstream f(file, std::ios::binary);
    if (!f)
        throw std::runtime_error("Error opening file: " + file);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void Progress(int p, int total) { // progress indicator, as i said so, its useless. but looks cool.
    static const char anim[] = "|/-\\";
    static int frame = 0;
    int bar_width = 30;
    int num_dashes = (p * bar_width) / total;
    int num_spaces = bar_width - num_dashes;
    std::cout << "\r[" << anim[frame] << "] "
        << std::string(num_dashes, '-') << p << "%"
        << std::string(num_spaces, ' ') << " [|]";
    std::cout.flush();
    frame = (frame + 1) % 4;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(600, 999);
    int wait = dis(gen);
    std::this_thread::sleep_for(std::chrono::microseconds(wait));
}

std::vector<uint8_t> a_xor(const std::vector<uint8_t>& input, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> out(input.size()); // better xor.
    size_t key_len = key.size();
    for (size_t i = 0; i < input.size(); ++i) {
        uint8_t m_key = key[i % key_len] ^ static_cast<uint8_t>(i & 0xFF);
        out[i] = input[i] ^ m_key;
    }
    return out;
}

std::vector<uint8_t> Compress(const std::vector<uint8_t>& in) { // mixed compression.
    std::vector<uint8_t> out;
    const size_t size = in.size();
    size_t pos = 0;
    std::vector<uint8_t> literals;
    std::vector<int> hash_table(65536, -1);
    auto L_Flush = [&]() {
        size_t literal_count = literals.size();
        size_t idx = 0;
        while (literal_count > 0) {
            uint8_t count = static_cast<uint8_t>(std::min(literal_count, static_cast<size_t>(127)));
            out.push_back(count);
            out.insert(out.end(), literals.begin() + idx, literals.begin() + idx + count);
            idx += count;
            literal_count -= count;
        }
        literals.clear();
        };

    while (pos < size) {
        size_t matched_len = 0, matched_off = 0;
        if (pos + MIN <= size) {
            uint32_t hash = (static_cast<uint32_t>(in[pos]) << 16) |
                (static_cast<uint32_t>(in[pos + 1]) << 8) |
                (static_cast<uint32_t>(in[pos + 2]));
            uint16_t h = static_cast<uint16_t>((hash ^ (hash >> 8)) & 0xFFFF);
            int candidate = hash_table[h];
            if (candidate >= 0 && pos - candidate <= WINDOW) {
                size_t curr_matched_len = 0;
                while (pos + curr_matched_len < size &&
                    in[candidate + curr_matched_len] == in[pos + curr_matched_len] &&
                    curr_matched_len < MAX &&
                    candidate + curr_matched_len < pos)
                {
                    curr_matched_len++;
                }
                if (curr_matched_len >= MIN) {
                    matched_len = curr_matched_len;
                    matched_off = pos - candidate;
                }
            }
            hash_table[h] = pos;
        }
        if (matched_len >= MIN) {
            if (!literals.empty())
                L_Flush();
            uint8_t token = static_cast<uint8_t>((matched_len - MIN) + 128);
            out.push_back(token);
            uint16_t off = static_cast<uint16_t>(matched_off);
            out.push_back(static_cast<uint8_t>((off >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(off & 0xFF));
            pos += matched_len;
        }
        else {
            literals.push_back(in[pos]);
            pos++;
            if (literals.size() == 127)
                L_Flush();
        }
    }
    if (!literals.empty())
        L_Flush();
    return out;
}

std::vector<uint8_t> Decompress(const std::vector<uint8_t>& in) { // decompression, but with freedom.
    std::vector<uint8_t> out;
    size_t pos = 0, size = in.size();
    while (pos < size) {
        uint8_t cmd = in[pos++];
        if (cmd < 128) {
            size_t count = cmd;
            if (pos + count > size)
                throw std::runtime_error("Not enough data during decompression.");
            out.insert(out.end(), in.begin() + pos, in.begin() + pos + count);
            pos += count;
        }
        else {
            size_t matched_length = (cmd - 128) + MIN;
            if (pos + 2 > size)
                throw std::runtime_error("Not enough data for match offset.");
            uint16_t offset = (static_cast<uint16_t>(in[pos]) << 8) | in[pos + 1];
            pos += 2;
            if (offset > out.size())
                throw std::runtime_error("Invalid offset during decompression.");
            size_t ref_pos = out.size() - offset;
            for (size_t i = 0; i < matched_length; i++)
                out.push_back(out[ref_pos + i]);
        }
    }
    return out;
}

std::vector<uint8_t> E_Entropy(const std::vector<uint8_t>& in) { // compress file entropy.
    uLongf destination_len = compressBound(in.size());
    std::vector<uint8_t> out(destination_len);
    int ret = compress2(out.data(), &destination_len, in.data(), in.size(), Z_BEST_COMPRESSION);
    if (ret != Z_OK)
        throw std::runtime_error("zlib error during compression: " + std::to_string(ret));
    out.resize(destination_len);
    return out;
}

std::vector<uint8_t> D_Entropy(const std::vector<uint8_t>& in, size_t original_size) { // decompress it.
    std::vector<uint8_t> out(original_size);
    uLongf destination_len = original_size;
    int ret = uncompress(out.data(), &destination_len, in.data(), in.size());
    if (ret != Z_OK)
        throw std::runtime_error("zlib error during decompression: " + std::to_string(ret));
    out.resize(destination_len);
    return out;
}

std::vector<uint8_t> generate_key(size_t len) { // file generation for extra layer. usual key size : 16
    std::vector<uint8_t> key(len);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);
    for (size_t i = 0; i < len; ++i)
        key[i] = static_cast<uint8_t>(dist(gen));
    return key;
}

std::vector<uint8_t> E_CompressFile(const std::vector<uint8_t>& in, const std::vector<uint8_t>& key) {
    auto lzss = Compress(in);
    auto entropy = E_Entropy(lzss);
    std::vector<uint8_t> payload;
    uint32_t s_lzss = static_cast<uint32_t>(lzss.size());
    payload.push_back(static_cast<uint8_t>(s_lzss & 0xFF));
    payload.push_back(static_cast<uint8_t>((s_lzss >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>((s_lzss >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((s_lzss >> 24) & 0xFF));
    payload.insert(payload.end(), entropy.begin(), entropy.end());
    auto encrypted = a_xor(payload, key);
    return encrypted;
}

std::vector<uint8_t> D_DecompressFile(const std::vector<uint8_t>& in, const std::vector<uint8_t>& key) {
    auto decrypted = a_xor(in, key);
    if (decrypted.size() < 4)
        throw std::runtime_error("Decrypted file data too short.");
    uint32_t S_LZSS = decrypted[0] | (decrypted[1] << 8) |
        (decrypted[2] << 16) | (decrypted[3] << 24);
    std::vector<uint8_t> entropy(decrypted.begin() + 4, decrypted.end());
    auto lzss = D_Entropy(entropy, S_LZSS);
    return Decompress(lzss);
}

void WriteArchiveHeader(std::vector<uint8_t>& out, const std::vector<uint8_t>& key, bool archive_folder) {
    out.insert(out.end(), MAGIC, MAGIC + MAGIC_SIZE);
    out.push_back(static_cast<uint8_t>(key.size()));
    for (uint8_t b : key)
        out.push_back(b ^ EKEY);
    out.push_back(archive_folder ? FOLDER_FLAG : 0);
}

void Readarchiveheader(std::vector<uint8_t>& data, std::vector<uint8_t>& key, bool& archive_folder) {
    if (data.size() < MAGIC_SIZE + 2)
        throw std::runtime_error("your data is too short for archive header.");
    if (memcmp(data.data(), MAGIC, MAGIC_SIZE) != 0)
        throw std::runtime_error("invalid magic(val) in archive header.");
    size_t pos = MAGIC_SIZE;
    uint8_t key_len = data[pos++];
    if (data.size() < pos + key_len + 1)
        throw std::runtime_error("your data is too short for key in archive header.");
    key.clear();
    for (size_t i = 0; i < key_len; ++i)
        key.push_back(data[pos + i] ^ EKEY);
    pos += key_len;
    archive_folder = (data[pos++] == FOLDER_FLAG);
    data.erase(data.begin(), data.begin() + pos);
}

std::vector<uint8_t> ArchiveFolderPayload(const fs::path& folder_path, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> payload;
    std::vector<fs::path> file_list;
    for (auto& entry : fs::recursive_directory_iterator(folder_path)) {
        if (fs::is_regular_file(entry))
            file_list.push_back(fs::relative(entry.path(), folder_path));
    }
    uint32_t archive_count = static_cast<uint32_t>(file_list.size());
    payload.push_back(static_cast<uint8_t>(archive_count & 0xFF));
    payload.push_back(static_cast<uint8_t>((archive_count >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>((archive_count >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((archive_count >> 24) & 0xFF));
    for (const auto& relative_path : file_list) {
        std::string path_str = relative_path.generic_string();
        uint32_t path_len = static_cast<uint32_t>(path_str.size());
        payload.push_back(static_cast<uint8_t>(path_len & 0xFF));
        payload.push_back(static_cast<uint8_t>((path_len >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>((path_len >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((path_len >> 24) & 0xFF));
        payload.insert(payload.end(), path_str.begin(), path_str.end());
        fs::path full_path = folder_path / relative_path;
        std::vector<uint8_t> file_data = Read(full_path.string());
        std::vector<uint8_t> comp_data = E_CompressFile(file_data, key);
        if (comp_data.size() < 4)
            throw std::runtime_error("internal error: compressed block too short.");
        payload.push_back(comp_data[0]);
        payload.push_back(comp_data[1]);
        payload.push_back(comp_data[2]);
        payload.push_back(comp_data[3]);
        uint32_t comp_size = static_cast<uint32_t>(comp_data.size());
        payload.push_back(static_cast<uint8_t>(comp_size & 0xFF));
        payload.push_back(static_cast<uint8_t>((comp_size >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>((comp_size >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((comp_size >> 24) & 0xFF));
        payload.insert(payload.end(), comp_data.begin(), comp_data.end());
    }
    return payload;
}

std::vector<uint8_t> BuildFolderArchive(const fs::path& folder_path, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> payload = ArchiveFolderPayload(folder_path, key);
    std::vector<uint8_t> payload2 = a_xor(payload, key); // xor again.
    std::vector<uint8_t> archive;
    WriteArchiveHeader(archive, key, true);
    archive.insert(archive.end(), payload2.begin(), payload2.end());
    return archive;
}

void ExtractFolderArchive(const std::string& archive_file, const fs::path& out_folder) {
    std::vector<uint8_t> archive_data = Read(archive_file);
    std::vector<uint8_t> key;
    bool folder_flag = false;
    Readarchiveheader(archive_data, key, folder_flag);
    if (!folder_flag)
        throw std::runtime_error("archive is not a folder archive.");
    std::vector<uint8_t> payload = a_xor(archive_data, key); // xor it, but not simple.
    size_t pos = 0;
    if (payload.size() < 4)
        throw std::runtime_error("payload is too short in folder archive.");
    uint32_t archive_count = payload[pos] | (payload[pos + 1] << 8) | (payload[pos + 2] << 16) | (payload[pos + 3] << 24);
    pos += 4;
    for (uint32_t i = 0; i < archive_count; i++) {
        if (pos + 4 > payload.size())
            throw std::runtime_error("corrupted archive: missing path length.");
        uint32_t path_length = payload[pos] | (payload[pos + 1] << 8) | (payload[pos + 2] << 16) | (payload[pos + 3] << 24);
        pos += 4;
        if (pos + path_length > payload.size())
            throw std::runtime_error("corrupted archive: missing path string.");
        std::string relative_path(reinterpret_cast<const char*>(&payload[pos]), path_length);
        pos += path_length;
        if (pos + 4 > payload.size())
            throw std::runtime_error("corrupted archive: missing original LZSS size.");
        pos += 4;
        if (pos + 4 > payload.size())
            throw std::runtime_error("corrupted archive: missing compressed block size.");
        uint32_t block_size = payload[pos] | (payload[pos + 1] << 8) | (payload[pos + 2] << 16) | (payload[pos + 3] << 24);
        pos += 4;
        if (pos + block_size > payload.size())
            throw std::runtime_error("corrupted archive: incomplete compressed block.");
        std::vector<uint8_t> comp_block(payload.begin() + pos, payload.begin() + pos + block_size);
        pos += block_size;
        std::vector<uint8_t> file_data = D_DecompressFile(comp_block, key);
        fs::path output_path = out_folder / relative_path;
        fs::create_directories(output_path.parent_path());
        Write(output_path.string(), file_data);
        std::cout << "Extracted: " << output_path.string() << "\n";
    }
}
std::vector<uint8_t> BuildFileArchive(const std::vector<uint8_t>& file_data, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> comp = E_CompressFile(file_data, key);
    std::vector<uint8_t> archive;
    WriteArchiveHeader(archive, key, false);
    archive.insert(archive.end(), comp.begin(), comp.end());
    return archive;
}
std::vector<uint8_t> ExtractFileArchive(const std::string& archive, const std::vector<uint8_t>& /*optkey*/) {
    std::vector<uint8_t> archive_data = Read(archive);
    std::vector<uint8_t> file_key;
    bool folder_flag = false;
    Readarchiveheader(archive_data, file_key, folder_flag);
    if (folder_flag)
        throw std::runtime_error("archive is a folder archive, not a file archive, try '-folder' instead.");
    return D_DecompressFile(archive_data, file_key);
}
