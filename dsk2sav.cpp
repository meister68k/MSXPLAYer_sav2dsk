//
// 2DDべたイメージファイルをMSX-Playerの.savファイルに変換する
//
// 2022-09-24 NAKAUE,T
//

// cl /std:c++17 dsk2sav.cpp

#include <algorithm>
#include <array>
#include <filesystem>                   // C++ 17
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace option {
    bool help_and_exit(false);
    std::string ext(".sav");
    size_t skip_sector = 1;
}


const size_t num_sectors = 2 * 80 * 9;  // 2面 * 80トラック/面 * 9セクタ/トラック
const size_t sector_size = 512;

class T_raw_data : public std::array<uint8_t, num_sectors * sector_size> {
public:
    size_t num_sectors;
    size_t sector_size;
};


// バッファ先頭から4バイトを32bit数値として取り出す
inline uint32_t get32bit(std::vector<uint8_t>::const_iterator p)
{
    return uint32_t(
        (uint32_t(p[3]) << 24) +
        (uint32_t(p[2]) << 16) +
        (uint32_t(p[1]) <<  8) +
        (uint32_t(p[0])      )
    );
}


// バッファ先頭から4バイトに32bit数値を書き込む
inline void put32bit(uint8_t* p, uint32_t num)
{
    p[0] = uint8_t( num        & 0xff);
    p[1] = uint8_t((num >>  8) & 0xff);
    p[2] = uint8_t((num >> 16) & 0xff);
    p[3] = uint8_t((num >> 24) & 0xff);

    return;
}


// BPBをバッファに書き込む
void write_bpb(T_raw_data* raw_data)
{
    std::array<uint8_t, 30> bpb = {
        0xeb, 0xfe, 0x90,                                   // ( 0- 2) ショートJMP 3バイト
        'U', 'N', 'K', 'N', 'O', 'W', 'N', ' ',             // ( 3- a) OEM名称 8バイト
        0x00, 0x02, 0x02, 0x01, 0x00,                       // ( b- f) BPB(セクタサイズ，クラスタサイズ，IPLセクタ数) 計5バイト
        0x02, 0x70, 0x00, 0xa0, 0x05,                       // (10-14) BPB(FAT個数，ディレクトリエントリ数，セクタ総数) 計5バイト
        0xf9, 0x03, 0x00, 0x09, 0x00,                       // (15-19) BPB(メディアID，FATサイズ，セクタ数/トラック) 計5バイト
        0x02, 0x00, 0x00, 0x00                              // (1a-1d) BPB(面数，隠しセクタ数) 計4バイト
    };

    std::copy(bpb.begin(), bpb.end(), raw_data->begin());

    return;
}


// 2DDのsav形式ファイルをバッファに読み出す
bool load_sav(fs::path target, T_raw_data* raw_data)
{
    std::ifstream ifs;
    ifs.open(target, std::ios::binary);
    if(!ifs) return(false);

    ifs.seekg(0, std::ios::end);
    size_t len = ifs.tellg();
    ifs.seekg(0);
    std::vector<uint8_t> sav_data(len);
    ifs.read(reinterpret_cast<char*>(sav_data.data()), sav_data.size());
    ifs.close();
    std::cout << sav_data.size() << " bytes" << std::endl;
    raw_data->fill(0);
    auto p_next = sav_data.begin();
    for(auto p = p_next; p < sav_data.end(); p = p_next) {
        auto sector_no = get32bit(p);
        p += 4;
        p_next = p + sector_size;
        std::cout << "sector " << sector_no << std::endl;
        auto dest = raw_data->begin() + sector_no * sector_size;
        std::copy(p, p_next, dest);
    }

    return true;
}


// 2DDのsav形式ファイルを書き出す
bool save_sav(fs::path outfile, T_raw_data &raw_data, size_t skip_sectors)
{
    std::ofstream ofs;
    ofs.open(outfile, std::ios::binary);
    if(!ofs) return(false);

    for(size_t sector_no = skip_sectors; sector_no < num_sectors; sector_no++) {
        if(((sector_no - skip_sectors) % 40) == 0) {
            std::cout << std::endl << "sector " << sector_no << " ";
        } else {
            std::cout << ".";
        }

        uint8_t buff[4];
        put32bit(buff, sector_no);
        ofs.write(reinterpret_cast<char*>(buff), 4);

        uint8_t* p = raw_data.data() + sector_no * sector_size;
        ofs.write(reinterpret_cast<char*>(p), sector_size);
    }

    return true;
}


// 2DDのべた形式ファイルをバッファに読み出す
bool load_dsk(fs::path target, T_raw_data &raw_data)
{
    std::ifstream ifs;
    ifs.open(target, std::ios::binary);
    if(!ifs) return(false);

    ifs.seekg(0, std::ios::end);
    size_t len = ifs.tellg();
    ifs.seekg(0);

    len = std::max(len, raw_data.size());

    raw_data.fill(0);
    ifs.read(reinterpret_cast<char*>(raw_data.data()), raw_data.size());
    ifs.close();

    std::cout << len << " bytes" << std::endl;

    return true;
}


bool sav2dsk(fs::path target, fs::path outfile)
{
    std::cout << target << " -> " << outfile << std::endl;

    T_raw_data raw_data;

    if(!load_sav(target, &raw_data)) {
        std::cerr << "load error : " << target << std::endl;
        return false;
    }

    write_bpb(&raw_data);

    std::ofstream ofs;
    ofs.open(outfile, std::ios::binary);
    if(ofs) {
        ofs.write(reinterpret_cast<char*>(raw_data.data()), raw_data.size());
    } else {
        std::cerr << "can't open : " << outfile << std::endl;
        return false;
    }

    return true;
}


// dsk形式からsav形式に変換して保存する
bool dsk2sav(fs::path target, fs::path outfile, size_t skip_sectors)
{
    std::cout << target << " -> " << outfile << std::endl;

    T_raw_data raw_data;

    if(!load_dsk(target, raw_data)) {
        std::cerr << "load error : " << target << std::endl;
        return false;
    }

    if(!save_sav(outfile, raw_data, skip_sectors)) {
        std::cerr << "save error : " << outfile << std::endl;
        return false;
    }

    return true;

}



int main(int argc, char *argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);

    for(auto it = args.begin(); it != args.end(); /* do nothing */) {
        std::cout << *it << std::endl;

        if(it->front() == '-') {
            auto opt = *it;
            args.erase(it);
            if(opt == "--help") {
                option::help_and_exit = true;
                break;
            } else if(opt == "--skip") {
                if(it == args.end()) {
                    option::help_and_exit = true;
                    break;
                }
                option::skip_sector = std::stoul(*it);
                args.erase(it);
            } else {
                option::help_and_exit = true;
                break;
            }
        } else {
            it++;
        }
    }

    if(option::help_and_exit || (args.size() == 0)) {
        std::cout <<
        "usage :" << std::endl <<
        "\tdsk2sav [--skip <num>] <filenames>" << std::endl <<
        std::endl <<
        "\t--skip\tskip sectors (default : 1)" << std::endl;

        return 1;
    }

    for(auto it = args.begin(); it != args.end(); it++) {
        fs::path target = *it;
        dsk2sav(*it, target.replace_extension(option::ext), option::skip_sector);
    }

    return 0;
}