#ifndef BOOST_TOOLS
#define BOOST_TOOLS
#include <boost/locale.hpp>

namespace boosttoolsnamespace
{
    class CBoostTools
    {
    public:

        //封装 string between( string const &text,string const &to_encoding,
        //string const &from_encoding,method_type how = default_method);
        //直接处理gbk转utf8编码
        static std::string gbktoutf8(std::string const &text);
        //直接处理utf8转gbk
        static std::string utf8togbk(std::string const &text);
        //直接处理big5转utf8
        static std::string big5toutf8(std::string const &text);
        //直接处理utf8转big5
        static std::string utf8tobig5(std::string const &text);
    };

}
#endif // BOOST_TOOLS

