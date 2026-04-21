#include "sig/sdp.h"
#include <sstream>
#include <algorithm>

// -----------------------------------------------------------------------
// Sdp::serialize()
// -----------------------------------------------------------------------

std::string Sdp::serialize() const {
    std::ostringstream ss;
    ss << "v=" << version << "\r\n";
    ss << "o=" << origin << "\r\n";
    ss << "s=" << session_name << "\r\n";
    ss << "t=0 0\r\n";

    for (const auto& m : media) {
        // m= line
        ss << "m=" << m.type << " " << m.port << " " << m.proto;
        for (int pt : m.fmts) ss << " " << pt;
        ss << "\r\n";

        // a= attributes
        if (!m.ice_ufrag.empty())
            ss << "a=ice-ufrag:" << m.ice_ufrag << "\r\n";
        if (!m.ice_pwd.empty())
            ss << "a=ice-pwd:" << m.ice_pwd << "\r\n";
        if (!m.fingerprint.empty())
            ss << "a=fingerprint:" << m.fingerprint << "\r\n";
        if (!m.candidate.empty())
            ss << "a=candidate:" << m.candidate << "\r\n";
        for (const auto& [pt, codec] : m.rtpmap)
            ss << "a=rtpmap:" << pt << " " << codec << "\r\n";
    }
    return ss.str();
}

// -----------------------------------------------------------------------
// Sdp::parse()
// -----------------------------------------------------------------------

std::optional<Sdp> Sdp::parse(const std::string& text) {
    Sdp sdp;
    SdpMedia* cur_media = nullptr;

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        // 去掉行尾 \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.size() < 2 || line[1] != '=')
            continue;

        char type = line[0];
        std::string value = line.substr(2);

        switch (type) {
        case 'v':
            sdp.version = value;
            break;
        case 'o':
            sdp.origin = value;
            break;
        case 's':
            sdp.session_name = value;
            break;
        case 'm': {
            sdp.media.emplace_back();
            cur_media = &sdp.media.back();
            // m=<type> <port> <proto> <fmt list>
            std::istringstream ms(value);
            std::string tok;
            ms >> cur_media->type;
            {
                uint16_t p = 0;
                ms >> p;
                cur_media->port = p;
            }
            ms >> cur_media->proto;
            int pt;
            while (ms >> pt)
                cur_media->fmts.push_back(pt);
            break;
        }
        case 'a':
            if (cur_media) {
                // a=rtpmap:PT codec/clock
                if (value.rfind("rtpmap:", 0) == 0) {
                    std::string rest = value.substr(7); // "PT codec/clock"
                    size_t sp = rest.find(' ');
                    if (sp != std::string::npos) {
                        int pt = std::stoi(rest.substr(0, sp));
                        std::string codec = rest.substr(sp + 1);
                        cur_media->rtpmap.emplace_back(pt, codec);
                    }
                } else if (value.rfind("fingerprint:", 0) == 0) {
                    cur_media->fingerprint = value.substr(12);
                } else if (value.rfind("ice-ufrag:", 0) == 0) {
                    cur_media->ice_ufrag = value.substr(10);
                } else if (value.rfind("ice-pwd:", 0) == 0) {
                    cur_media->ice_pwd = value.substr(8);
                } else if (value.rfind("candidate:", 0) == 0) {
                    if (cur_media->candidate.empty())
                        cur_media->candidate = value.substr(10);
                }
            }
            break;
        default:
            break;
        }
    }

    return sdp;
}
