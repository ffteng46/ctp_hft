#include "boost_tools.h"

using namespace boost::locale::conv;

//直接处理gbk转utf8编码
std::string boosttoolsnamespace::CBoostTools::gbktoutf8(std::string const &text)
{
    //"UTF-8", "GBK"
    std::string const &to_encoding("UTF-8");
    std::string const &from_encoding("GBK");
    method_type how = default_method;
    return boost::locale::conv::between(text.c_str(), text.c_str() + text.size(), to_encoding, from_encoding, how);
}
//直接处理utf8转gbk
std::string boosttoolsnamespace::CBoostTools::utf8togbk(std::string const &text)
{
    std::string const &to_encoding("GBK");
    std::string const &from_encoding("UTF-8");
    method_type how = default_method;
    return boost::locale::conv::between(text.c_str(), text.c_str() + text.size(), to_encoding, from_encoding, how);
}
//直接处理big5转utf8
std::string boosttoolsnamespace::CBoostTools::big5toutf8(std::string const &text)
{
    std::string const &to_encoding("UTF-8");
    std::string const &from_encoding("BIG5");
    method_type how = default_method;
    return boost::locale::conv::between(text.c_str(), text.c_str() + text.size(), to_encoding, from_encoding, how);
}
//直接处理utf8转big5
std::string boosttoolsnamespace::CBoostTools::utf8tobig5(std::string const &text)
{
    std::string const &to_encoding("BIG5");
    std::string const &from_encoding("UTF-8");
    method_type how = default_method;
    return boost::locale::conv::between(text.c_str(), text.c_str() + text.size(), to_encoding, from_encoding, how);
}
