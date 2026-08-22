#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

struct Token {
    enum class Kind { Ident, String, Number, LBrace, RBrace, LBracket, RBracket, Colon, Semicolon, End } kind;
    std::string text;
    std::size_t line{};
};

class Lexer {
public:
    explicit Lexer(std::string_view input) : input_(input) {}

    Token next() {
        skip_ws_comments();
        if (pos_ >= input_.size()) return {Token::Kind::End, {}, line_};
        const char c = input_[pos_];
        switch (c) {
            case '{': ++pos_; return {Token::Kind::LBrace, "{", line_};
            case '}': ++pos_; return {Token::Kind::RBrace, "}", line_};
            case '[': ++pos_; return {Token::Kind::LBracket, "[", line_};
            case ']': ++pos_; return {Token::Kind::RBracket, "]", line_};
            case ':': ++pos_; return {Token::Kind::Colon, ":", line_};
            case ';': ++pos_; return {Token::Kind::Semicolon, ";", line_};
            case '"': return string_token();
            default: break;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) return number_token();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return ident_token();
        throw std::runtime_error("lexer: unexpected character at line " + std::to_string(line_));
    }

private:
    void skip_ws_comments() {
        while (pos_ < input_.size()) {
            if (input_[pos_] == '\n') { ++line_; ++pos_; continue; }
            if (std::isspace(static_cast<unsigned char>(input_[pos_]))) { ++pos_; continue; }
            if (input_[pos_] == '/' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '/') {
                pos_ += 2;
                while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
                continue;
            }
            break;
        }
    }

    Token ident_token() {
        const auto start = pos_++;
        while (pos_ < input_.size() && (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) ++pos_;
        return {Token::Kind::Ident, std::string(input_.substr(start, pos_ - start)), line_};
    }

    Token number_token() {
        const auto start = pos_++;
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        return {Token::Kind::Number, std::string(input_.substr(start, pos_ - start)), line_};
    }

    Token string_token() {
        ++pos_;
        std::string out;
        while (pos_ < input_.size() && input_[pos_] != '"') {
            if (input_[pos_] == '\n') throw std::runtime_error("lexer: newline in string literal");
            out.push_back(input_[pos_++]);
        }
        if (pos_ >= input_.size()) throw std::runtime_error("lexer: unterminated string");
        ++pos_;
        return {Token::Kind::String, std::move(out), line_};
    }

    std::string_view input_;
    std::size_t pos_{};
    std::size_t line_{1};
};

struct Field {
    std::string name;
    std::string type;
    std::size_t width{};
    std::size_t offset{};
};

struct Message {
    std::string name;
    char type{};
    std::vector<Field> fields;
    std::size_t size{1}; // type byte is implicit.
};

struct Protocol {
    std::string name;
    std::vector<Message> messages;
};

class Parser {
public:
    explicit Parser(std::string_view input) : lexer_(input), current_(lexer_.next()) {}

    Protocol parse() {
        expect_ident("protocol");
        Protocol p;
        p.name = expect(Token::Kind::Ident).text;
        expect(Token::Kind::Semicolon);
        std::unordered_set<char> message_types;
        std::unordered_set<std::string> message_names;
        while (current_.kind != Token::Kind::End) {
            expect_ident("message");
            Message m;
            m.name = expect(Token::Kind::Ident).text;
            const auto type = expect(Token::Kind::String).text;
            if (type.size() != 1) fail("message type must be exactly one byte");
            m.type = type[0];
            if (!message_types.insert(m.type).second) fail("duplicate message type");
            if (!message_names.insert(m.name).second) fail("duplicate message name");
            expect(Token::Kind::LBrace);
            std::unordered_set<std::string> field_names;
            while (current_.kind != Token::Kind::RBrace) {
                Field f;
                f.name = expect(Token::Kind::Ident).text;
                if (!field_names.insert(f.name).second) fail("duplicate field: " + f.name);
                expect(Token::Kind::Colon);
                f.type = expect(Token::Kind::Ident).text;
                if (f.type == "ascii") {
                    expect(Token::Kind::LBracket);
                    f.width = std::stoul(expect(Token::Kind::Number).text);
                    expect(Token::Kind::RBracket);
                    if (f.width == 0) fail("ascii width must be > 0");
                } else {
                    f.width = type_width(f.type);
                }
                f.offset = m.size;
                m.size += f.width;
                expect(Token::Kind::Semicolon);
                m.fields.push_back(std::move(f));
            }
            expect(Token::Kind::RBrace);
            p.messages.push_back(std::move(m));
        }
        return p;
    }

private:
    static std::size_t type_width(const std::string& type) {
        if (type == "u8" || type == "char") return 1;
        if (type == "u16_be") return 2;
        if (type == "u32_be" || type == "price4") return 4;
        if (type == "u48_be") return 6;
        if (type == "u64_be" || type == "price8") return 8;
        throw std::runtime_error("parser: unsupported type " + type);
    }

    void expect_ident(std::string_view text) {
        auto tok = expect(Token::Kind::Ident);
        if (tok.text != text) fail("expected '" + std::string(text) + "'");
    }

    Token expect(Token::Kind kind) {
        if (current_.kind != kind) fail("unexpected token '" + current_.text + "'");
        Token out = current_;
        current_ = lexer_.next();
        return out;
    }

    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error("parser line " + std::to_string(current_.line) + ": " + msg);
    }

    Lexer lexer_;
    Token current_;
};

std::string snake_namespace(std::string name) {
    std::string out;
    for (char c : name) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

std::string accessor_type(const Field& f) {
    if (f.type == "u8") return "std::uint8_t";
    if (f.type == "u16_be") return "std::uint16_t";
    if (f.type == "u32_be" || f.type == "price4") return "std::uint32_t";
    if (f.type == "u48_be" || f.type == "u64_be" || f.type == "price8") return "std::uint64_t";
    if (f.type == "char") return "char";
    if (f.type == "ascii") return "std::string_view";
    throw std::runtime_error("codegen: unsupported type " + f.type);
}

std::string accessor_expr(const Field& f) {
    const std::string off = std::to_string(f.offset);
    if (f.type == "u8") return "efe::read_u8(bytes_, " + off + ")";
    if (f.type == "u16_be") return "efe::read_u16_be(bytes_, " + off + ")";
    if (f.type == "u32_be" || f.type == "price4") return "efe::read_u32_be(bytes_, " + off + ")";
    if (f.type == "u48_be") return "efe::read_u48_be(bytes_, " + off + ")";
    if (f.type == "u64_be" || f.type == "price8") return "efe::read_u64_be(bytes_, " + off + ")";
    if (f.type == "char") return "efe::read_char(bytes_, " + off + ")";
    if (f.type == "ascii") return "efe::read_ascii_view(bytes_, " + off + ", " + std::to_string(f.width) + ")";
    throw std::runtime_error("codegen: unsupported type " + f.type);
}

void emit(const Protocol& p, const std::filesystem::path& output) {
    std::filesystem::create_directories(output.parent_path());
    std::ofstream out(output);
    if (!out) throw std::runtime_error("cannot open output: " + output.string());
    const std::string ns = snake_namespace(p.name);
    out << "// AUTO-GENERATED by efe_protocolc. Do not edit by hand.\n";
    out << "#pragma once\n\n";
    out << "#include \"efe/byte_reader.hpp\"\n";
    out << "#include <cstddef>\n#include <cstdint>\n#include <optional>\n#include <span>\n#include <string_view>\n\n";
    out << "namespace efe::generated::" << ns << " {\n\n";
    for (const auto& m : p.messages) {
        out << "class " << m.name << "View {\npublic:\n";
        out << "    static constexpr char message_type = '" << (m.type == '\'' ? '\\' : m.type) << "';\n";
        out << "    static constexpr std::size_t encoded_size = " << m.size << ";\n";
        out << "    explicit " << m.name << "View(std::span<const std::uint8_t> bytes) : bytes_(bytes) {\n";
        out << "        if (bytes_.size() != encoded_size || bytes_[0] != static_cast<std::uint8_t>(message_type)) throw efe::DecodeError(\"invalid " << m.name << " message\");\n";
        out << "    }\n";
        for (const auto& f : m.fields) {
            out << "    [[nodiscard]] " << accessor_type(f) << " " << f.name << "() const { return " << accessor_expr(f) << "; }\n";
        }
        out << "private:\n    std::span<const std::uint8_t> bytes_;\n};\n\n";
    }
    out << "inline std::optional<std::size_t> message_size(char type) noexcept {\n    switch (type) {\n";
    for (const auto& m : p.messages) out << "        case '" << m.type << "': return " << m.name << "View::encoded_size;\n";
    out << "        default: return std::nullopt;\n    }\n}\n\n";
    out << "inline std::string_view message_name(char type) noexcept {\n    switch (type) {\n";
    for (const auto& m : p.messages) out << "        case '" << m.type << "': return \"" << m.name << "\";\n";
    out << "        default: return \"Unknown\";\n    }\n}\n\n";
    out << "}  // namespace efe::generated::" << ns << "\n";
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open input: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "usage: efe_protocolc <schema.efe> <output.hpp>\n";
            return 2;
        }
        const auto source = slurp(argv[1]);
        const auto protocol = Parser(source).parse();
        emit(protocol, argv[2]);
        std::cout << "generated " << protocol.messages.size() << " message views -> " << argv[2] << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "protocolc error: " << e.what() << '\n';
        return 1;
    }
}
