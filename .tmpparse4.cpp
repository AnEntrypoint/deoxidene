#include "C:/dev/ctest/include/deoxidene/result.hpp"
#include <cstring>
#include <cstdio>
namespace{
enum class ParseError{kEmpty,kNotNumeric};
auto parse_positive_int(const char* text) -> deoxidene::Result<int, ParseError> {
  if (std::strlen(text) == 0) return deoxidene::Fail<ParseError,int>(ParseError::kEmpty);
  int value = 0;
  for (const char* p = text; *p != 0; ++p) {
    if (*p < 48 || *p > 57) return deoxidene::Fail<ParseError,int>(ParseError::kNotNumeric);
    value = (value * 10) + (*p - 48);
  }
  return deoxidene::Ok<int,ParseError>(value);
}
}
int main(){
  auto r1=parse_positive_int(""); std::printf("t1 has_value=%d\n",(int)r1.has_value());
  auto r2=parse_positive_int("0"); std::printf("t2 has_value=%d val=%d\n",(int)r2.has_value(), r2.has_value()?r2.value():-1);
  auto r3=parse_positive_int("2147483647"); std::printf("t3 has_value=%d val=%d\n",(int)r3.has_value(), r3.has_value()?r3.value():-1);
  auto r4=parse_positive_int("12a"); std::printf("t4 has_value=%d\n",(int)r4.has_value());
  auto r5=parse_positive_int("999999999999"); std::printf("t5 has_value=%d val=%d\n",(int)r5.has_value(), r5.has_value()?r5.value():-1);
  return 0;
}