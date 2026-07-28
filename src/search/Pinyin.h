#pragma once

#include <QString>

namespace Pinyin {

// First-letter string for Chinese characters in the name (non-CJK kept as-is, lowercased).
QString initialsOf(const QString &text);

// True if needle (ascii) is a subsequence/substring of pinyin initials of text.
bool initialsContain(const QString &text, const QString &needle, bool caseSensitive);

} // namespace Pinyin
