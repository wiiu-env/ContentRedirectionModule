#include "StringTools.h"
#include <algorithm>

void SafeReplaceInString(std::string &subject, std::string search, const std::string &replace) {
    if (search.empty() || search == replace) {
        return; // Avoid infinite loops and invalid input
    }
    std::string lowerSubject = subject;

    std::ranges::transform(subject, lowerSubject.begin(), ::tolower);
    std::ranges::transform(search, search.begin(), ::tolower);

    if (const size_t pos = lowerSubject.find(search); pos != std::string::npos) {
        subject.replace(pos, search.length(), replace);
    }
}