#pragma once
// Small bounded JSON reader for untrusted provider responses. No executable content.
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <charconv>
#include <initializer_list>

namespace StrictJson
{
    struct Value
    {
        enum class Kind { Object, Array, String, Number, Boolean, Null } kind = Kind::Null;
        std::map<std::string, Value> object;
        std::vector<Value> array;
        std::string scalar;
        const Value& At(const std::string& key) const
        {
            if (kind != Kind::Object || !object.count(key)) throw std::runtime_error("Missing field: " + key);
            return object.at(key);
        }
        std::string String() const
        {
            if (kind != Kind::String) throw std::runtime_error("Expected string");
            return scalar;
        }
        int Integer() const
        {
            int n = 0;
            auto r = std::from_chars(scalar.data(), scalar.data() + scalar.size(), n);
            if (kind != Kind::Number || r.ec != std::errc{} || r.ptr != scalar.data() + scalar.size())
                throw std::runtime_error("Expected integer");
            return n;
        }
        void Fields(std::initializer_list<const char*> keys) const
        {
            if (kind != Kind::Object || object.size() != keys.size()) throw std::runtime_error("Unexpected or missing fields");
            for (auto key : keys) At(key);
        }
    };
    inline void Utf8(std::string& s, unsigned n)
    {
        if(n < 128) s += char(n);
        else if(n < 2048) { s += char(192 | (n >> 6)); s += char(128 | (n & 63)); }
        else if(n < 65536) { s += char(224 | (n >> 12)); s += char(128 | ((n >> 6) & 63)); s += char(128 | (n & 63)); }
        else { s += char(240 | (n >> 18)); s += char(128 | ((n >> 12) & 63)); s += char(128 | ((n >> 6) & 63)); s += char(128 | (n & 63)); }
    }
    inline std::string Quote(const std::string& s)
    {
        std::string out = "\"";
        const char* hex = "0123456789abcdef";
        for(unsigned char c : s)
        {
            if(c == '"' || c == '\\') { out += '\\'; out += char(c); }
            else if(c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
            else out += char(c);
        }
        return out + "\"";
    }
    class Reader
    {
        const std::string& s; size_t p = 0; size_t nodes = 0;
        size_t maximumBytes, maximumNodes;
        [[noreturn]] void Bad() const { throw std::runtime_error("Invalid JSON at byte " + std::to_string(p)); }
        void Ws() { while(p < s.size() && (s[p]==' ' || s[p]=='\r' || s[p]=='\n' || s[p]=='\t')) ++p; }
        bool Eat(char c) { Ws(); if(p < s.size() && s[p]==c) { ++p; return true; } return false; }
        unsigned Hex()
        {
            unsigned n = 0;
            for(int i=0;i<4;++i) { if(p==s.size()) Bad(); char c=s[p++]; int v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; if(v<0) Bad(); n=n*16+v; }
            return n;
        }
        std::string Str()
        {
            if(!Eat('"')) Bad(); std::string out;
            while(p<s.size())
            {
                unsigned char c=s[p++];
                if(c=='"') return out;
                if(c<32) Bad();
                if(c!='\\')
                {
                    if(c<128) { out += char(c); continue; }
                    unsigned n=0, count=0, minimum=0;
                    if(c>=194 && c<=223) { n=c&31; count=1; minimum=128; }
                    else if(c>=224 && c<=239) { n=c&15; count=2; minimum=2048; }
                    else if(c>=240 && c<=244) { n=c&7; count=3; minimum=65536; } else Bad();
                    for(unsigned i=0;i<count;++i) { if(p==s.size()) Bad(); unsigned char b=s[p++]; if((b&192)!=128) Bad(); n=(n<<6)|(b&63); }
                    if(n<minimum || n>0x10ffff || (n>=0xd800 && n<=0xdfff)) Bad();
                    Utf8(out,n); continue;
                }
                if(p==s.size()) Bad();
                switch(s[p++])
                {
                case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
                case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;case 'b':out+='\b';break;case 'f':out+='\f';break;
                case 'u': { unsigned n=Hex(); if(n>=0xd800&&n<=0xdbff) { if(p+2>s.size()||s[p]!='\\'||s[p+1]!='u') Bad(); p+=2; unsigned low=Hex(); if(low<0xdc00||low>0xdfff) Bad(); n=0x10000+((n-0xd800)<<10)+(low-0xdc00); } else if(n>=0xdc00&&n<=0xdfff) Bad(); Utf8(out,n); break; }
                default:Bad();
                }
            }
            Bad();
        }
        Value Read(unsigned depth)
        {
            if(depth>24 || ++nodes>maximumNodes) Bad(); Ws(); if(p==s.size()) Bad(); Value v;
            if(s[p]=='{') { ++p;v.kind=Value::Kind::Object;if(Eat('}'))return v; do { auto key=Str();if(!Eat(':'))Bad(); if(v.object.count(key))Bad();v.object.emplace(key,Read(depth+1));if(Eat('}'))return v; }while(Eat(','));Bad(); }
            if(s[p]=='[') { ++p;v.kind=Value::Kind::Array;if(Eat(']'))return v;do {v.array.push_back(Read(depth+1));if(Eat(']'))return v;}while(Eat(','));Bad(); }
            if(s[p]=='"') {v.kind=Value::Kind::String;v.scalar=Str();return v;}
            for(auto word : {"true","false","null"}) { auto len=std::string(word).size();if(s.compare(p,len,word)==0) {p+=len;v.kind=std::string(word)=="null"?Value::Kind::Null:Value::Kind::Boolean;v.scalar=word;return v;} }
            size_t start=p; if(s[p]=='-')++p; if(p==s.size())Bad();
            if(s[p]=='0')++p; else {if(s[p]<'1'||s[p]>'9')Bad();while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;}
            if(p<s.size()&&s[p]=='.') {++p;size_t b=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(b==p)Bad();}
            if(p<s.size()&&(s[p]=='e'||s[p]=='E')) {++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;size_t b=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(b==p)Bad();}
            v.kind=Value::Kind::Number;v.scalar=s.substr(start,p-start);return v;
        }
    public:
        explicit Reader(const std::string& text,size_t maxBytes=1024*1024,size_t maxNodes=12000):s(text),maximumBytes(maxBytes),maximumNodes(maxNodes) {}
        Value Parse() {if(s.size()>maximumBytes) throw std::runtime_error("JSON size limit exceeded");auto v=Read(0);Ws();if(p!=s.size())Bad();return v;}
    };
    inline Value Parse(const std::string& s) { return Reader(s).Parse(); }
    inline std::string Serialize(const Value& v)
    {
        if(v.kind==Value::Kind::String)return Quote(v.scalar);
        if(v.kind==Value::Kind::Null)return "null";
        if(v.kind==Value::Kind::Number||v.kind==Value::Kind::Boolean)return v.scalar;
        std::string out=v.kind==Value::Kind::Object?"{":"[";bool first=true;
        if(v.kind==Value::Kind::Object)for(const auto& item:v.object){if(!first)out+=',';first=false;out+=Quote(item.first)+':'+Serialize(item.second);}
        else for(const auto& item:v.array){if(!first)out+=',';first=false;out+=Serialize(item);}
        return out+(v.kind==Value::Kind::Object?"}":"]");
    }
}
