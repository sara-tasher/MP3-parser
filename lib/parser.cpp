#include "parser.h"

bool IsBitSet(char chr, size_t bit) {
    return ((chr >> bit) & 1) == 1;
}

Header ReadHeader(std::ifstream& in) {
    Header header;
    in.read(header.file_id.data(), HEADER_FILE_ID_SIZE);
    if (header.file_id != "ID3") {
        std::cout << header.file_id << '\n';
        std::cerr << "INCORRECT FILE\n";
        exit(1);
    }

    in.read(header.version.data(), HEADER_VERSION_SIZE);

    char flags;
    in.read(&flags, HEADER_FLAGS_SIZE);
    if (IsBitSet(flags, 7)) header.unsync = true;
    if (IsBitSet(flags, 6)) header.ext_header = true;
    if (IsBitSet(flags, 5)) header.exp_ind = true;
    if (IsBitSet(flags, 4)) header.footer = true;
    header.size = ReadSize(in);

    if(header.ext_header){
        size_t a = ReadSize(in);
        in.seekg(a - 4, in.cur);
    }
    return header;
}

size_t ReadSize(std::ifstream& in) {
    std::string size(4, ' ');
    in.read(size.data(), size.size());

    return size[0] << 21 | size[1] << 14 | size[2] << 7 | size[3];
}

std::string ReadDataToZeroByte(std::ifstream& in, size_t encoding) {
    std::string data;
    char byte;
    while (in.read(&byte, 1)) {
        if (byte == 0x00) break;
        data += byte;
    }

    if (encoding == 0x00) {
        return ISO_8859_TO_UTF_8(data);
    }

    return data;
}

std::ostream& operator<<(std::ostream& out, const Frame& frame) {
    out << "\n";
    frame.Print(out);
    return out;
}

std::ifstream& operator>>(std::ifstream& in, Frame& frame) {
    frame.Read(in);
    return in;
}

void Parse(const std::string& file) {
    std::ifstream in( file, std::ios::binary);

    if (!in.is_open()) {
        std::cerr << "No such file to open\n";
        exit(-1);
    }

    Header header = ReadHeader(in);

    Frame* frame;
    size_t cur_byte = 0;
    size_t padding_size = 0;
    while (cur_byte < header.size) {
        std::string frame_id(4, ' ');
        in.read(frame_id.data(), 4);

        if (frame_id[0] == 0x00) {
            padding_size += 4;
            char byte;
            while (in.read(&byte, 1)) {
                if (byte != 0x00) break;
                padding_size++;
            }
            in.seekg(-1, in.cur);
            break;
        }

        if (frame_id == "TXXX") {
            frame = new TXXXFrame(in);
        } else if (frame_id[0] == 'T') {
            frame = new TextFrame(in);
        } else if (frame_id == "COMM") {
            frame = new CommentFrame(in);
        } else if (frame_id == "POPM") {
            frame = new PopularimeterFrame(in);
        } else  if (frame_id == "USLT") {
            frame = new TranscriptionFrame(in);
        } else if (frame_id == "WXXX") {
            frame = new WXXXrame(in);
        } else if (frame_id[0] == 'W') {
            frame = new URLFrame(in);
        } else if (frame_id == "PCNT") {
            frame = new PlayCounterFrame(in);
        } else if (frame_id == "PRIV") {
            frame = new PrivateFrame(in);
        } else if (frame_id == "GRID") {
            frame = new GroupIdFrame(in);
        } else if (frame_id == "ETCO") {
            frame = new ETCOFrame(in);
        } else if (frame_id == "SYLT") {
            frame = new SYLTFrame(in);
        } else if (frame_id == "COMR") {
            frame = new COMRFrame(in, file);
        } else if (frame_id == "ENCR") {
            frame = new ENCRFrame(in);
        } else if (frame_id == "EQU2") {
            frame = new EQU2Frame(in);
        } else if (frame_id == "LINK") {
            frame = new LINKFrame(in);
        } else if (frame_id == "OWNE") {
            frame = new OWNEFrame(in);
        } else if (frame_id == "POSS") {
            frame = new POSSFrame(in);
        } else if (frame_id == "RBUF") {
            frame = new RBUFFrame(in);
        } else if (frame_id == "RVA2") {
            frame = new RVA2Frame(in);
        } else if (frame_id == "SEEK") {
            frame = new SEEKFrame(in);
        } else if (frame_id == "UFID") {
            frame = new UFIDFrame(in);
        } else if (frame_id == "USER") {
            frame = new USERFrame(in);
        } else {
            std::cout << "Didn't understand \"" << frame_id << "\" frame\n";
            break;
        }

        in >> *frame;
        std::cout << *frame;
        cur_byte += frame->Size();
        delete frame;
    }

    in.seekg(-10, std::ios::end);
    std::string footer_id(3, ' ');
    in.read(footer_id.data(), 3);
    if (footer_id == "3DI") {
        std::cout << "Here is footer\n";
    }
}

void ReadData(char encoding, std::ifstream& in, std::string& data) {
    size_t size = data.size();

    if (encoding == 0x01 || encoding == 0x02) {
        if (encoding == 0x01) {
            in.seekg(2, std::ios::cur);
            size -= 2;
        }

        std::u16string u16(size / 2 + 1, '\0');
        in.read((char*)&u16[0], size);
        data = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.to_bytes(u16);
    } else {
        in.read(data.data(), size);
    }

    if (encoding == 0x00) {
        data =  ISO_8859_TO_UTF_8(data);
    }
}



std::string ISO_8859_TO_UTF_8(const std::string &str) {
    std::string res;
    for (const unsigned char i : str) {
        if (i < 0x80) {
            res.push_back(i);
        } else {
            res.push_back(0xc0 | i >> 6);
            res.push_back(0x80 | (i & 0x3f));
        }
    }
    return res;
}

uint32_t GetTime(std::ifstream& in) {
    char byte;
    uint32_t time = 0;
    for (int i = 0; i < 4; ++i) {
        byte = in.get();
        time |= static_cast<unsigned char>(byte) << ((3 - i) * 8);
    }

    return time;
}

std::string EncodingToText(size_t encoding) {
    switch (encoding) {
        case 0x00:
            return "ISO-8859-1 [ISO-8859-1]. Terminated with $00.";
        case 0x01:
            return "UTF-16 [UTF-16] encoded Unicode [UNICODE] with BOM. All \
            strings in the same frame SHALL have the same byteorder. \
            Terminated with $00 00.";
        case 0x02:
            return "UTF-16BE [UTF-16] encoded Unicode [UNICODE] without BOM. \
            Terminated with $00 00.";
        case 0x03:
            return "UTF-8 [UTF-8] encoded Unicode [UNICODE]. Terminated with $00.";
        default:
            return "Incorrect encoding.";
    }
}

