/*
 * Copyright (c) 2026
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/loader/text_inst_object.hh"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

#include "base/logging.hh"

namespace gem5
{

namespace loader
{

namespace
{

TextInstObjectFormat textInstObjectFormat;

constexpr Addr DefaultLoadAddr = 0x10000;

bool
endsWith(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string
trim(const std::string &str)
{
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";

    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string
stripComment(const std::string &line)
{
    const auto hash = line.find('#');
    const auto semi = line.find(';');
    const auto slash = line.find("//");
    auto pos = std::min(hash, semi);
    pos = std::min(pos, slash);

    return pos == std::string::npos ? line : line.substr(0, pos);
}

uint64_t
parseHex(const std::string &text, const std::string &filename, int line_no,
         uint64_t max)
{
    std::string token = text;
    token.erase(std::remove(token.begin(), token.end(), '_'), token.end());

    const char *start = token.c_str();
    char *end = nullptr;
    errno = 0;
    const uint64_t value = std::strtoull(start, &end, 16);

    fatal_if(start == end || *end != '\0' || errno == ERANGE || value > max,
             "%s:%d: invalid hex value '%s'.", filename.c_str(), line_no,
             text.c_str());

    return value;
}

} // anonymous namespace

TextInstObject::TextInstObject(ImageFileDataPtr ifd, Addr load_addr,
                               std::vector<uint8_t> bytes)
    : ObjectFile(ifd), image(std::move(bytes))
{
    arch = Riscv64;
    opSys = Linux;
    byteOrder = ByteOrder::little;
    entry = load_addr;
}

MemoryImage
TextInstObject::buildImage() const
{
    return {{ "text_inst", entry, image.data(), image.size() }};
}

ObjectFile *
TextInstObjectFormat::load(ImageFileDataPtr data)
{
    const std::string &filename = data->filename();
    if (!endsWith(filename, ".txt") && !endsWith(filename, ".hex"))
        return nullptr;

    const std::string contents(reinterpret_cast<const char *>(data->data()),
                               data->len());
    std::istringstream input(contents);
    std::string line;
    std::vector<uint8_t> bytes;
    Addr load_addr = DefaultLoadAddr;
    bool saw_load_addr = false;
    int line_no = 0;

    while (std::getline(input, line)) {
        line_no++;
        std::string text = trim(stripComment(line));
        if (text.empty())
            continue;

        if (text[0] == '@') {
            fatal_if(saw_load_addr || !bytes.empty(),
                     "%s:%d: load address must appear before instructions.",
                     filename.c_str(), line_no);
            load_addr = parseHex(trim(text.substr(1)), filename, line_no,
                                 MaxAddr);
            saw_load_addr = true;
            continue;
        }

        const uint32_t inst = parseHex(text, filename, line_no, UINT32_MAX);
        bytes.push_back(inst & 0xff);
        bytes.push_back((inst >> 8) & 0xff);
        bytes.push_back((inst >> 16) & 0xff);
        bytes.push_back((inst >> 24) & 0xff);
    }

    fatal_if(bytes.empty(), "%s: no RISC-V instructions found.",
             filename.c_str());

    inform("Loaded %d RISC-V text instructions from %s at %#x",
           static_cast<int>(bytes.size() / sizeof(uint32_t)),
           filename.c_str(), load_addr);

    return new TextInstObject(data, load_addr, std::move(bytes));
}

} // namespace loader
} // namespace gem5
