#pragma once

#include<iostream>

namespace Utility
{
    inline bool is_number(const std::string &str)
    {
        for (auto c : str)
        {
            if (c < '0' || c > '9')
                return false;
        }
        return true;
    }
} // namespace Utility
