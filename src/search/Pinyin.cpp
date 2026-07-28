#include "search/Pinyin.h"

#include <QChar>
#include <QHash>

namespace Pinyin {

static char initialForCode(uint unicode)
{
    static const struct {
        uint start;
        uint end;
        char initial;
    } ranges[] = {
        {0x4E00, 0x4F7F, 'a'}, {0x4F80, 0x50FF, 'b'}, {0x5100, 0x52FF, 'c'},
        {0x5300, 0x54FF, 'd'}, {0x5500, 0x56FF, 'e'}, {0x5700, 0x58FF, 'f'},
        {0x5900, 0x5AFF, 'g'}, {0x5B00, 0x5CFF, 'h'}, {0x5D00, 0x5EFF, 'j'},
        {0x5F00, 0x60FF, 'k'}, {0x6100, 0x62FF, 'l'}, {0x6300, 0x64FF, 'm'},
        {0x6500, 0x66FF, 'n'}, {0x6700, 0x68FF, 'p'}, {0x6900, 0x6AFF, 'q'},
        {0x6B00, 0x6CFF, 'r'}, {0x6D00, 0x6EFF, 's'}, {0x6F00, 0x70FF, 't'},
        {0x7100, 0x72FF, 'w'}, {0x7300, 0x74FF, 'x'}, {0x7500, 0x76FF, 'y'},
        {0x7700, 0x9FFF, 'z'},
    };

    for (const auto &r : ranges) {
        if (unicode >= r.start && unicode <= r.end)
            return r.initial;
    }
    return 0;
}

static char initialOverride(uint u)
{
    static QHash<uint, char> map;
    if (map.isEmpty()) {
        map.insert(0x6587, 'w'); // 文
        map.insert(0x6863, 'd'); // 档
        map.insert(0x4E0B, 'x'); // 下
        map.insert(0x8F7D, 'z'); // 载
        map.insert(0x56FE, 't'); // 图
        map.insert(0x7247, 'p'); // 片
        map.insert(0x89C6, 's'); // 视
        map.insert(0x9891, 'p'); // 频
        map.insert(0x97F3, 'y'); // 音
        map.insert(0x4E50, 'l'); // 乐
        map.insert(0x4E66, 's'); // 书
        map.insert(0x7C7B, 'l'); // 类
        map.insert(0x684C, 'z'); // 桌
        map.insert(0x9762, 'm'); // 面
        map.insert(0x6848, 'a'); // 案
        map.insert(0x9879, 'x'); // 项
        map.insert(0x76EE, 'm'); // 目
        map.insert(0x5F55, 'l'); // 录
        map.insert(0x5DE5, 'g'); // 工
        map.insert(0x4F5C, 'z'); // 作
        map.insert(0x533A, 'q'); // 区
        map.insert(0x57DF, 'y'); // 域
        map.insert(0x7F51, 'w'); // 网
        map.insert(0x7EDC, 'l'); // 络
        map.insert(0x4EF6, 'j'); // 件
        map.insert(0x5939, 'j'); // 夹
    }
    return map.value(u, 0);
}

QString initialsOf(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const uint u = ch.unicode();
        if (u >= 0x4E00 && u <= 0x9FFF) {
            char c = initialOverride(u);
            if (!c)
                c = initialForCode(u);
            if (c)
                out.append(QLatin1Char(c));
        } else if (ch.isLetterOrNumber()) {
            out.append(ch.toLower());
        }
    }
    return out;
}

bool initialsContain(const QString &text, const QString &needle, bool caseSensitive)
{
    if (needle.isEmpty())
        return true;
    const QString ini = initialsOf(text);
    if (caseSensitive)
        return ini.contains(needle);
    return ini.contains(needle.toLower());
}

} // namespace Pinyin
