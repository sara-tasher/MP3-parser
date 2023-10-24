#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <codecvt>
#include <vector>
#include <cstdint>
#include <locale>

const uint8_t HEADER_FLAGS_SIZE = 1;
const uint8_t ENCODING_SIZE = 1;
const uint8_t FLAGS_SIZE = 2;
const uint8_t HEADER_VERSION_SIZE = 2;
const uint8_t HEADER_FILE_ID_SIZE = 3;
const uint8_t LANGUAGE_SIZE = 3;
const uint8_t FRAME_ID_SIZE = 4;
const uint8_t DATE_SIZE = 8;

struct Header {
    Header() : unsync(false), ext_header(false), exp_ind(false), footer(false), size(0) {
        file_id.resize(HEADER_FILE_ID_SIZE);
        version.resize(HEADER_VERSION_SIZE);
    }

    std::string file_id;
    std::string version;
    bool unsync;
    bool ext_header;
    bool exp_ind;
    bool footer;
    size_t size;
};

void Parse(const std::string& file);

void ReadData(char encoding, std::ifstream& in, std::string& data);

Header ReadHeader(std::ifstream& in);

size_t ReadSize(std::ifstream& in);

uint32_t GetTime(std::ifstream& in);

std::string ReadDataToZeroByte(std::ifstream& in, size_t encoding);

std::string ISO_8859_TO_UTF_8(const std::string& str);

bool IsBitSet(char chr, size_t bit);

std::string EncodingToText(size_t encoding);

inline std::string EventToDescription(size_t event) {
    switch (event) {
        case 0x00:
            return "padding (has no meaning)";
        case 0x01:
            return "end of initial silence";
        case 0x02:
            return "intro start";
        case 0x03:
            return "main part start";
        case 0x04:
            return "outro start";
        case 0x05:
            return "outro end";
        case 0x06:
            return "verse start";
        case 0x07:
            return "refrain start";
        case 0x08:
            return "interlude start";
        case 0x09:
            return "theme start";
        case 0x0A:
            return "variation start";
        case 0x0B:
            return "key change";
        case 0x0C:
            return "time change";
        case 0x0D:
            return "momentary unwanted noise (Snap, Crackle & Pop)";
        case 0x0E:
            return "sustained noise";
        case 0x0F:
            return "sustained noise end";
        case 0x10:
            return "intro end";
        case 0x11:
            return "main part end";
        case 0x12:
            return "verse end";
        case 0x13:
            return "refrain end";
        case 0x14:
            return "theme end";
        case 0x15:
            return "profanity";
        case 0x16:
            return "profanity end";
        case 0xFD:
            return "audio end (start of silence)";
        case 0xFE:
            return "audio file ends";
        case 0xFF:
            return "one more byte of events follows (all the following bytes with \
                the value $FF have the same function)";
        default:
            break;
    }

    if (0x17 <= event && event <= 0xDF) {
        return "reserved for future use";
    } else if (0xE0 <= event && event <= 0xEF) {
        return "not predefined synch 0-F";
    } else if (0xF0 <= event && event <= 0xFC) {
        return "reserved for future use";
    }

    return "unknown event";
}

class Frame {
public:
    Frame(std::ifstream& in) : size(ReadSize(in)) {
        flags.resize(FLAGS_SIZE);
        in.read(flags.data(), flags.size());
    }

    virtual ~Frame() = default;

    size_t Size() const {
        return size + 10;
    }
    friend std::ifstream& operator>>(std::ifstream& in, Frame& frame);
    friend std::ostream& operator<<(std::ostream& out, const Frame& frame);
protected:
    virtual void Read(std::ifstream& in) = 0;
    virtual void Print(std::ostream& out) const = 0;
    std::string type;
    std::string flags;
    size_t size;
};

std::ifstream& operator>>(std::ifstream& in, Frame& frame);

std::ostream& operator<<(std::ostream& out, const Frame& frame);


class LanguageFrame : public Frame {
public:
    LanguageFrame(std::ifstream& in) : Frame(in) {
        language.resize(LANGUAGE_SIZE);
    }
protected:
    char encoding;
    std::string language;
    std::string desc;
    std::string data;
};

class TextFrame: public Frame {
public:
    TextFrame(std::ifstream& in) : Frame(in) {
        type = "Text Frame";
    }

protected:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        if (encoding == 0x01) {
            in.seekg(ENCODING_SIZE, in.cur);
        }

        size_t cur_byte = 1;
        std::string cur;
        while (cur_byte < size) {
            cur += in.get();
            cur_byte++;
            if (encoding == 0x03) {
                if (cur.size() >= 2 && cur.back() == 0x00 && cur[cur.size() - 2] == 0x00) {
                    data.push_back(cur);
                    cur.clear();
                }
            } else {
                if (cur.back() == 0x00) {
                    data.push_back(cur);
                    cur.clear();
                }
            }
        }

        if (cur.size() != 0) {
            data.push_back(cur);
        }
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Encoding is: " << EncodingToText(encoding) << '\n';
        out << "Size: " << size << '\n';
        out << "Content: \n";
        for (const auto& i : data) {
            std::cout << i << ' ';
        }
        std::cout << '\n';
    }

    char encoding;
    std::vector<std::string> data;
};

class TXXXFrame: public TextFrame {
public:
    TXXXFrame(std::ifstream& in) : TextFrame(in) {}
private:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        data.push_back(ReadDataToZeroByte(in, 0x03));
        value.resize(size - 1 - data[0].size() - 1);
        in.read(value.data(), value.size());
    }

    void Print(std::ostream& out) const override {
        out << "This is: " << type << '\n';
        out << "Encoding is: " << EncodingToText(encoding) << '\n';
        out << "Content: " << data[0] << '\n';
        out << "Value: " << value << '\n';
        std::cout << '\n';
    }

    std::string value;
};


class CommentFrame: public LanguageFrame {
public:
    CommentFrame(std::ifstream& in) : LanguageFrame(in) {
        type = "Comment Frame";
    }

private:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        in.read(language.data(), language.size());
        desc = ReadDataToZeroByte(in, encoding);
        data.resize(size - ENCODING_SIZE - language.size() - desc.size() - 1);
        ReadData(encoding, in, data);
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Encoding: " << EncodingToText(encoding) << '\n';
        out << "Language: " << language << '\n';
        out << "Description: " << desc << '\n';
        out << "Data: " << data << '\n' << '\n';
    }
};


class PopularimeterFrame: public Frame {
public:
    PopularimeterFrame(std::ifstream& in) : Frame(in) {
        type = "Popularimeter Frame";
    }

private:
    void Read(std::ifstream& in) override {
        email = ReadDataToZeroByte(in, 0x03);
        rating = in.get();
        counter.resize(size - email.size() - 1 - 1);
        in.read(counter.data(), counter.size());
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Email: " << email << '\n';
        out << "Rating: " << static_cast<int>(rating) << '\n';
        out << "Counter: " << counter << '\n' << '\n';
    }

    char rating;
    std::string email;
    std::string counter;
};

class TranscriptionFrame: public LanguageFrame {
public:
    TranscriptionFrame(std::ifstream& in) : LanguageFrame(in) {
        type = "Transcription Frame";
    }

private:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        in.read(language.data(), language.size());
        desc = ReadDataToZeroByte(in, encoding);
        data.resize(size - 1 - language.size() - desc.size() - 1);
        ReadData(encoding, in, data);
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Language: " << language << '\n' ;
        out << "Content: " << desc << '\n' ;
        out << "Text: " << data << '\n' << '\n';
    }
};

class URLFrame: public Frame {
public:
    URLFrame(std::ifstream& in) : Frame(in) {
        url.resize(size);
        type = "URL Frame";
    }

protected:
    void Read(std::ifstream& in) override {
        in.read(url.data(), url.size());
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "URL: " << url << '\n' << '\n';
    }

    std::string url;
};

class WXXXrame: public URLFrame {
public:
    WXXXrame(std::ifstream& in) : URLFrame(in) {
        type = "URL Frame";
    }

protected:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        desc = ReadDataToZeroByte(in, encoding);
        url.resize(size - ENCODING_SIZE - desc.size() - 1);
        in.read(url.data(), url.size());
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "URL: " << url << '\n' << '\n';
        out << "Description: " << desc << '\n';
    }

    char encoding;
    std::string desc;
};


class PlayCounterFrame: public Frame {
public:
    PlayCounterFrame(std::ifstream& in) : Frame(in) {
        type = "Play Counter Frame";
    }

private:
    void Read(std::ifstream& in) override {
        counter.resize(size);
        in.read(counter.data(), size);
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Counter: " << counter << '\n';
    }

    std::string counter;
};


class PrivateFrame: public Frame {
public:
    PrivateFrame(std::ifstream& in) : Frame(in) {
        type = "Private Frame";
    }

private:
    void Read(std::ifstream& in) override {
        owner_id = ReadDataToZeroByte(in, 0x03);
        private_data.resize(size - owner_id.size() - 1);
        in.read(private_data.data(), private_data.size());
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Owner ID: " << owner_id << '\n';
    }

    std::string owner_id;
    std::string private_data;
};


class GroupIdFrame: public Frame {
public:
    GroupIdFrame(std::ifstream& in) : Frame(in) {
        type = "Group ID Frame";
    }

private:
    void Read(std::ifstream& in) override {
        owner_id = ReadDataToZeroByte(in, 0x03);
        group_symbol = in.get();
        group_data.resize(size - owner_id.size() - 1);
        in.read(group_data.data(), group_data.size());
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Owner ID: " << owner_id << '\n';
        out << "Group symbol: " << group_symbol << '\n';
        out << "Group data: " << group_data << '\n';
    }

    std::string owner_id;
    char group_symbol;
    std::string group_data;
};


class ETCOFrame: public Frame {
public:
    ETCOFrame(std::ifstream& in) : Frame(in) {
        type = "ETCO Frame";
    }

private:
    void Read(std::ifstream& in) override {
        time_stamp_format = in.get();

        size_t cur_byte = 1;
        while (cur_byte < size) {
            char event;
            event = in.get();
            uint32_t time = GetTime(in);
            cur_byte += 5;
            data.push_back({event, time});
        }
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        for (const auto& i : data) {
            std::cout << EventToDescription(i.first) << ' ' << i.second << '\n';
        }
        std::cout << '\n';
    }

    char time_stamp_format;
    std::vector<std::pair<char, uint32_t>> data;
};

class SYLTFrame: public LanguageFrame {
public:
    SYLTFrame(std::ifstream& in) : LanguageFrame(in) {
        type = "SYLT Frame";
    }

private:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        in.read(language.data(), language.size());
        time_stamp_format = in.get();
        content_type = in.get();
        desc = ReadDataToZeroByte(in, 0x03);

        size_t cur_byte = ENCODING_SIZE + LANGUAGE_SIZE + 1 + 1 + desc.size() + 1;
        char byte;
        while (cur_byte < size) {
            std::string lyrics = ReadDataToZeroByte(in, 0x03);
            uint32_t time = GetTime(in);

            time_data.push_back({time, lyrics});
            cur_byte += 4 + lyrics.size() + 1;
        }

    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        std::cout << "Language: " << language << '\n';
        std::cout << "Content descriptor: " << desc << '\n';
        for (const auto& i : time_data) {
            std::cout << i.first << ' ' << i.second << '\n';
        }
        std::cout << '\n';
    }

    char time_stamp_format;
    char content_type;
    std::vector<std::pair<uint32_t, std::string>> time_data;
};


class COMRFrame: public Frame {
public:
    COMRFrame(std::ifstream& in, const std::string& file_) : Frame(in) {
        type = "COMR Frame";
        file = file_;
    }

private:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        price = ReadDataToZeroByte(in, encoding);
        valid_until = ReadDataToZeroByte(in, encoding);
        contact = ReadDataToZeroByte(in, encoding);
        recieved_as = in.get();
        seller = ReadDataToZeroByte(in, encoding);
        desc = ReadDataToZeroByte(in, encoding);
        MIME = ReadDataToZeroByte(in, encoding);

        size_t delim_pos = MIME.find('/');
        std::string type = delim_pos == std::string::npos ? ".undefined" : "." + MIME.substr(delim_pos + 1);
        std::ofstream logo(file + type, std::ios::binary);
        size_t bytes_left = size - ENCODING_SIZE - price.size() - 1 - valid_until.size() - 1 -
                            contact.size() - 1 - 1 - seller.size() - 1 - desc.size() - 1 - MIME.size() - 1;
        for (size_t i = 0; i < bytes_left; ++i) {
            char byte = in.get();
            logo.write(&byte, 1);
        }
    }

    void Print(std::ostream& out) const override  {
        out << "This is: " << type << '\n';
        out << "Price: " << price << '\n';
        out << "Seller: " << seller << '\n';
        out << "Description: " << desc << '\n';
    }

    char encoding;
    char recieved_as;
    std::string price;
    std::string valid_until;
    std::string contact;
    std::string seller;
    std::string desc;
    std::string MIME;
    std::string file;
};



class ENCRFrame: public Frame {
public:
    ENCRFrame(std::ifstream& in) : Frame(in) {
        type = "ENCR Frame";
    }

private:
    void Read(std::ifstream& in) override {
        owner_id = ReadDataToZeroByte(in, 0x03);
        method = in.get();
        std::ofstream out("secret_data");
        for (size_t i = 0; i < size - owner_id.size() - 1 - 1; ++i) {
            char byte = in.get();
            out.write(&byte, 1);
        }
    }

    void Print(std::ostream& out) const override  {
        out << "Type: " << type << '\n';
        out << "Owner id: " << owner_id << '\n';
    }

    std::string owner_id;
    char method;
};


class EQU2Frame: public Frame {
public:
    EQU2Frame(std::ifstream& in) : Frame(in) {
        type = "EQU2 Frame";
        freq = 0;
        volume = 0;
    }

private:
    void Read(std::ifstream& in) override {
        interpolation_method = in.get();
        id = ReadDataToZeroByte(in, 0x03);
        for (size_t i = 0; i < 2; ++i) {
            char byte = in.get();
            freq |= static_cast<unsigned char> (byte) << ((1 - i) * 8);
        }
        for (size_t i = 0; i < 2; ++i) {
            char byte = in.get();
            volume |= static_cast<unsigned char> (byte) << ((1 - i) * 8);
        }
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Interpolation method " << interpolation_method << '\n';
        std::cout << "Identification " << id << '\n';
        std::cout << "Frequency and volume " << freq << ' ' << volume << '\n';

    }

    char interpolation_method;
    std::string id;
    uint16_t freq;
    uint16_t volume;
};


class LINKFrame: public Frame {
public:
    LINKFrame(std::ifstream& in) : Frame(in) {
        type = "LINK Frame";
        id.resize(FRAME_ID_SIZE);
    }

private:
    void Read(std::ifstream& in) override {
        std::cout << "Type: " << type << '\n';
        in.read(id.data(), id.size());
        url = ReadDataToZeroByte(in, 0x03);
        size_t cur_byte = id.size() + url.size() + 1;
        while (cur_byte < size) {
            data.push_back(ReadDataToZeroByte(in, 0x03));
            cur_byte += data.back().size();
        }
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "ID: " << id << '\n';
        std::cout << "URL: " << url << '\n';
        std::cout << "Data: \n";
        for (const auto& i : data) {
            std::cout << i << '\n';
        }
        std::cout << '\n';
    }

    std::string id;
    std::string url;
    std::vector<std::string> data;
};


class OWNEFrame: public Frame {
public:
    OWNEFrame(std::ifstream& in) : Frame(in) {
        type = "OWNE Frame";
        date.resize(DATE_SIZE);
    }

private:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        paid = ReadDataToZeroByte(in, encoding);
        in.read(date.data(), date.size());
        seller.resize(size - ENCODING_SIZE - paid.size() - 1 - date.size());
        ReadData(encoding, in, seller);
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Price paid: " << paid << '\n';
        std::cout << "Date <YYYYMMDD>: " << date << '\n';
        std::cout << "Seller: " << seller << '\n';
    }

    char encoding;
    std::string paid;
    std::string date;
    std::string seller;
};


class POSSFrame: public Frame {
public:
    POSSFrame(std::ifstream& in) : Frame(in) {
        type = "POSS Frame";
    }

private:
    void Read(std::ifstream& in) override {
        time_stamp_format = in.get();
        position = GetTime(in);
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Time stamp format: " << time_stamp_format << '\n';
        std::cout << "Position of something: " << position << '\n';
    }

    char time_stamp_format;
    uint32_t position;
};


class RBUFFrame: public Frame {
public:
    RBUFFrame(std::ifstream& in) : Frame(in) {
        type = "RBUF Frame";
    }

private:
    void Read(std::ifstream& in) override {
        buffer_size = GetTime(in);
        char byte = in.get();
        embedded_info_flag = (bool)byte;
        offset = GetTime(in);
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Buffer size: " << buffer_size << '\n';
        std::cout << "Offset: " << offset << '\n';
    }

    uint32_t buffer_size;
    bool embedded_info_flag;
    size_t offset;
};


class RVA2Frame: public Frame {
public:
    RVA2Frame(std::ifstream& in) : Frame(in) {
        type = "RVA2 Frame";
        volume = 0;
    }

private:
    void Read(std::ifstream& in) override {
        channel_type = in.get();
        for (size_t i = 0; i < 2; ++i) {
            char byte = in.get();
            volume |= (unsigned char) byte << ((1 - i) * 8);
        }
        bits_representing_peak = in.get();
        peak_volume = GetTime(in);
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Channel type: " << ' ' << channel_type << '\n';
        std::cout << "Volume: " << volume << '\n';
        std::cout << "Peak volume: " << peak_volume << '\n';
    }

    char channel_type;
    uint16_t volume;
    char bits_representing_peak;
    uint32_t peak_volume;
};


class SEEKFrame: public Frame {
public:
    SEEKFrame(std::ifstream& in) : Frame(in) {
        type = "SEEK Frame";
    }

private:
    void Read(std::ifstream& in) override {
        offset = GetTime(in);
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Offset: " << offset << '\n';
    }

    size_t offset;
};

class UFIDFrame: public Frame {
public:
    UFIDFrame(std::ifstream& in) : Frame(in) {
        type = "UFID Frame";
    }

private:
    void Read(std::ifstream& in) override {
        owner_id = ReadDataToZeroByte(in, 0x03);
        id.resize(size - owner_id.size() - 1);
        in.read(id.data(), id.size());
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Owner id: " << owner_id << '\n';

    }

    std::string owner_id;
    std::string id;
};


class USERFrame: public LanguageFrame {
public:
    USERFrame(std::ifstream& in) : LanguageFrame(in) {
        type = "USER Frame";
    }

private:
    void Read(std::ifstream& in) override {
        encoding = in.get();
        in.read(language.data(), language.size());
        data.resize(size - 1 - language.size());
        in.read(data.data(), data.size());
    }

    void Print(std::ostream& out) const override  {
        std::cout << "Type: " << type << '\n';
        std::cout << "Encoding: " << EncodingToText(encoding) << '\n';
        std::cout << "Language: " << language << '\n';
        std::cout << "Data: " << data << '\n';
    }
};