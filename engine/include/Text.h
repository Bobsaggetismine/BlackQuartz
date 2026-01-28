#pragma once
#include <string>
#include <vector>
namespace bq::text {
	inline std::vector<std::string> split(const std::string& input, const std::string delimeter) 
	{
        size_t pos_start = 0, pos_end, delim_len = delimeter.length();
        std::string token;
        std::vector<std::string> res;

        while ((pos_end = input.find(delimeter, pos_start)) != std::string::npos) {
            token = input.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back(token);
        }

        res.push_back(input.substr(pos_start));
        return res;
	}
}