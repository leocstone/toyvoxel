#include "ansi.h"

std::string ANSI::escape(const char* text, int modifier, int color)
{
    std::stringstream ss;
    ss << "\033[" << modifier << ";" << color << "m" << text << "\033[0m";
    return ss.str();
}

std::string ANSI::red(const char* text)
{
    return escape(text, 0, FG_RED);
}
