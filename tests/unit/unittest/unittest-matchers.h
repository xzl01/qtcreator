// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <gmock/gmock-matchers.h>

#include <utils/smallstringio.h>

namespace Internal {

template <typename StringType>
class EndsWithMatcher
{
 public:
    explicit EndsWithMatcher(const StringType& suffix) : m_suffix(suffix) {}

    template <typename CharType>
    bool MatchAndExplain(CharType *s, testing::MatchResultListener *listener) const
    {
        return s != NULL && MatchAndExplain(StringType(s), listener);
    }


    template <typename MatcheeStringType>
    bool MatchAndExplain(const MatcheeStringType& s,
                         testing::MatchResultListener* /* listener */) const
    {
        return s.endsWith(m_suffix);
    }

    void DescribeTo(::std::ostream* os) const
    {
        *os << "ends with " << m_suffix;
    }

    void DescribeNegationTo(::std::ostream* os) const
    {
        *os << "doesn't end with " << m_suffix;
    }

    EndsWithMatcher(EndsWithMatcher const &) = default;
    EndsWithMatcher &operator=(EndsWithMatcher const &) = delete;

private:
    const StringType m_suffix;
};

class QStringEndsWithMatcher
{
public:
    explicit QStringEndsWithMatcher(const QString &suffix)
        : m_suffix(suffix)
    {}

    template<typename MatcheeStringType>
    bool MatchAndExplain(const MatcheeStringType &s, testing::MatchResultListener * /* listener */) const
    {
        return s.endsWith(m_suffix);
    }

    void DescribeTo(::std::ostream *os) const
    {
        *os << "ends with " << testing::PrintToString(m_suffix);
    }

    void DescribeNegationTo(::std::ostream *os) const
    {
        *os << "doesn't end with " << testing::PrintToString(m_suffix);
    }

private:
    const QString m_suffix;
};

class IsEmptyMatcher : public testing::internal::IsEmptyMatcher
{
public:
    using Base = testing::internal::IsEmptyMatcher;

    using Base::MatchAndExplain;

    bool MatchAndExplain(const QString &s, testing::MatchResultListener *listener) const
    {
        if (s.isEmpty()) {
            return true;
        }

        *listener << "whose size is " << s.size();
        return false;
    }

    void DescribeTo(std::ostream *os) const { *os << "is empty"; }

    void DescribeNegationTo(std::ostream *os) const { *os << "isn't empty"; }
};

} // namespace Internal

inline auto EndsWith(const Utils::SmallString &suffix)
{
    return Internal::EndsWithMatcher(suffix);
}

inline auto EndsWith(const QStringView &suffix)
{
    return ::testing::PolymorphicMatcher(Internal::QStringEndsWithMatcher(suffix.toString()));
}

inline auto IsEmpty()
{
    return ::testing::PolymorphicMatcher(Internal::IsEmptyMatcher());
}
