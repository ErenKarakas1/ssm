#ifndef SSM_SSM_HPP
#define SSM_SSM_HPP

#include <string_view>

namespace ssm {

inline constexpr std::string_view SNIPPETS_DIRNAME = ".snippets";

bool create_snippet(std::string_view name);

void list_snippets();

bool remove_snippet(std::string_view name);

bool get_snippet(std::string_view name);
bool get_snippet(int number);

bool edit_snippet(std::string_view name);
bool edit_snippet(int number);

} // namespace ssm

#endif //SSM_SSM_HPP
