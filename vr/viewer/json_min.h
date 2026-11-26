// Syndicate Wars VR viewer: a minimal JSON reader, for the asset metadata
// (models/scale.json, effects/effects.json of the assets repository).
// Reads objects, arrays, numbers, strings, true/false/null; no escapes
// beyond \" \\ \/ \n \t (the files have none else).
#pragma once

#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    double num = 0;
    bool b = false;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj; // in file order

    bool has(const std::string &k) const { return get(k) != nullptr; }
    const Json *get(const std::string &k) const
    {
        for (const auto &kv : obj)
            if (kv.first == k)
                return &kv.second;
        return nullptr;
    }
    // Member k as a number, or def.
    double n(const std::string &k, double def = 0) const
    {
        const Json *j = get(k);
        return (j && j->type == NUM) ? j->num : def;
    }
    std::string s(const std::string &k, const std::string &def = "") const
    {
        const Json *j = get(k);
        return (j && j->type == STR) ? j->str : def;
    }
    const Json &operator[](size_t i) const { return arr[i]; }
    size_t size() const { return type == ARR ? arr.size() : obj.size(); }
};

class JsonParser {
public:
    static bool parse(const std::string &text, Json &out)
    {
        JsonParser p(text);
        p.ws();
        return p.value(out);
    }

    static bool parse_file(const std::string &path, Json &out)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr)
            return false;
        std::string text;
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            text.append(buf, n);
        fclose(f);
        return parse(text, out);
    }

private:
    explicit JsonParser(const std::string &t) : t_(t) {}

    void ws()
    {
        while (i_ < t_.size() && (t_[i_] == ' ' || t_[i_] == '\n' || t_[i_] == '\r' || t_[i_] == '\t'))
            i_++;
    }

    bool lit(const char *w)
    {
        size_t n = strlen(w);
        if (t_.compare(i_, n, w) != 0)
            return false;
        i_ += n;
        return true;
    }

    bool string(std::string &out)
    {
        if (t_[i_] != '"')
            return false;
        i_++;
        while (i_ < t_.size() && t_[i_] != '"') {
            char c = t_[i_++];
            if (c == '\\' && i_ < t_.size()) {
                char e = t_[i_++];
                c = e == 'n' ? '\n' : e == 't' ? '\t' : e;
            }
            out += c;
        }
        if (i_ >= t_.size())
            return false;
        i_++;
        return true;
    }

    bool value(Json &v)
    {
        if (i_ >= t_.size())
            return false;
        char c = t_[i_];
        if (c == '{') {
            v.type = Json::OBJ;
            i_++;
            ws();
            if (t_[i_] == '}') {
                i_++;
                return true;
            }
            for (;;) {
                ws();
                std::string k;
                if (!string(k))
                    return false;
                ws();
                if (t_[i_] != ':')
                    return false;
                i_++;
                ws();
                v.obj.emplace_back(k, Json());
                if (!value(v.obj.back().second))
                    return false;
                ws();
                if (t_[i_] == ',') {
                    i_++;
                    continue;
                }
                if (t_[i_] == '}') {
                    i_++;
                    return true;
                }
                return false;
            }
        }
        if (c == '[') {
            v.type = Json::ARR;
            i_++;
            ws();
            if (t_[i_] == ']') {
                i_++;
                return true;
            }
            for (;;) {
                ws();
                v.arr.emplace_back();
                if (!value(v.arr.back()))
                    return false;
                ws();
                if (t_[i_] == ',') {
                    i_++;
                    continue;
                }
                if (t_[i_] == ']') {
                    i_++;
                    return true;
                }
                return false;
            }
        }
        if (c == '"') {
            v.type = Json::STR;
            return string(v.str);
        }
        if (lit("true")) {
            v.type = Json::BOOL;
            v.b = true;
            return true;
        }
        if (lit("false")) {
            v.type = Json::BOOL;
            return true;
        }
        if (lit("null"))
            return true;
        char *end = nullptr;
        v.num = strtod(t_.c_str() + i_, &end);
        if (end == t_.c_str() + i_)
            return false;
        v.type = Json::NUM;
        i_ = end - t_.c_str();
        return true;
    }

    const std::string &t_;
    size_t i_ = 0;
};
