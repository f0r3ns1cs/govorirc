#include "Ui.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

static constexpr short kColorMap[16] = {
    COLOR_WHITE,   // 0  white
    COLOR_BLACK,   // 1  black
    COLOR_BLUE,    // 2  blue
    COLOR_GREEN,   // 3  green
    COLOR_RED,     // 4  red
    COLOR_RED,     // 5  brown (no brown in terminal, so just default back to red)
    COLOR_MAGENTA, // 6  magenta
    COLOR_YELLOW,  // 7  orange  (default to yellow)
    COLOR_YELLOW,  // 8  yellow
    COLOR_GREEN,   // 9  light green
    COLOR_CYAN,    // 10 cyan
    COLOR_CYAN,    // 11 light cyan
    COLOR_BLUE,    // 12 light blue
    COLOR_MAGENTA, // 13 pink
    COLOR_BLACK,   // 14 grey
    COLOR_WHITE,   // 15 light grey
};

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static int parseDigits(std::string_view s, size_t& pos) 
{
    if (pos >= s.size() || !isDigit(s[pos])) return -1;
    int n = s[pos++] - '0';
    if (pos < s.size() && isDigit(s[pos]))
        n = n * 10 + (s[pos++] - '0');
    return n;
}

size_t Ui::utf8CharLen(unsigned char first) 
{
    if (first < 0x80) return 1;
    if ((first & 0xE0) == 0xC0) return 2;
    if ((first & 0xF0) == 0xE0) return 3;
    if ((first & 0xF8) == 0xF0) return 4;
    return 1; // single byte
}

size_t Ui::utf8DisplayWidth(std::string_view s) 
{
    size_t cols = 0;
    for (size_t i = 0; i < s.size(); i += utf8CharLen(static_cast<unsigned char>(s[i])))
        ++cols;
    return cols;
}

Ui::Ui() 
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
#ifndef _WIN32
        use_default_colors();
#endif
        hasColor_ = true;
        for (short i = 0; i < 16; ++i) {
            init_pair(i + 1, kColorMap[i], -1);
        }
    }

    int rows = 0, cols = 0;
    getmaxyx(stdscr, rows, cols);
    if (rows < 3) rows = 3;

    msgs_   = newwin(rows - 2, cols, 0, 0);
    status_ = newwin(1, cols, rows - 2, 0);
    input_  = newwin(1, cols, rows - 1, 0);

    scrollok(msgs_, TRUE);
    keypad(input_, TRUE);
    nodelay(input_, TRUE);

    if (hasColor_) wbkgd(status_, A_REVERSE);
}

Ui::~Ui() 
{
    if (msgs_)   delwin(msgs_);
    if (status_) delwin(status_);
    if (input_)  delwin(input_);
    endwin();
}

void Ui::addLine(std::string body) 
{
    lines_.push_back(std::move(body));
    if (lines_.size() > 10000) lines_.erase(lines_.begin(), lines_.begin() + 1000);
    dirty_ = true;
}

void Ui::setStatus(std::string s) 
{
    statusText_ = std::move(s);
    dirty_ = true;
}

void Ui::handleResize() 
{
#ifndef _WIN32
    endwin();
    refresh();
#endif
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    if (rows < 3) rows = 3;

    if (msgs_)   delwin(msgs_);
    if (status_) delwin(status_);
    if (input_)  delwin(input_);

    msgs_   = newwin(rows - 2, cols, 0, 0);
    status_ = newwin(1, cols, rows - 2, 0);
    input_  = newwin(1, cols, rows - 1, 0);

    scrollok(msgs_, TRUE);
    keypad(input_, TRUE);
    nodelay(input_, TRUE);
    if (hasColor_) wbkgd(status_, A_REVERSE);

    dirty_ = true;
}

void Ui::tick() 
{
    int ch;
    while ((ch = wgetch(input_)) != ERR) {
        handleKey(ch);
        if (stop_) return;
    }
    if (dirty_) redraw();
}

void Ui::napIfIdle(int ms) const
{
    if (dirty_) return;
    napms(ms);
}

void Ui::handleKey(int ch) 
{
    switch (ch) {
    case '\n':
    case KEY_ENTER: {
        if (inputBuf_.empty()) return;
        std::string submitted = std::move(inputBuf_);
        inputBuf_.clear();
        cursorByte_ = 0;
        historyIdx_ = 0;
        dirty_ = true;

        history_.push_back(submitted);
        if (history_.size() > 200) history_.erase(history_.begin());

        if (submitted == "/quit") {
            stop_ = true;
            return;
        }
        if (lineHandler_) lineHandler_(submitted);
        return;
    }

    case KEY_BACKSPACE:
    case 127:
    case 8: {
        if (cursorByte_ == 0) return;
        // Step back over a full UTF-8 character.
        size_t back = cursorByte_ - 1;
        while (back > 0 && (static_cast<unsigned char>(inputBuf_[back]) & 0xC0) == 0x80)
            --back;
        inputBuf_.erase(back, cursorByte_ - back);
        cursorByte_ = back;
        dirty_ = true;
        return;
    }

    case KEY_DC: {
        if (cursorByte_ >= inputBuf_.size()) return;
        size_t len = utf8CharLen(static_cast<unsigned char>(inputBuf_[cursorByte_]));
        inputBuf_.erase(cursorByte_, len);
        dirty_ = true;
        return;
    }

    case KEY_LEFT: {
        if (cursorByte_ == 0) return;
        size_t back = cursorByte_ - 1;
        while (back > 0 && (static_cast<unsigned char>(inputBuf_[back]) & 0xC0) == 0x80)
            --back;
        cursorByte_ = back;
        dirty_ = true;
        return;
    }

    case KEY_RIGHT: {
        if (cursorByte_ >= inputBuf_.size()) return;
        cursorByte_ += utf8CharLen(static_cast<unsigned char>(inputBuf_[cursorByte_]));
        dirty_ = true;
        return;
    }

    case KEY_HOME:
        if (cursorByte_ != 0) cursorByte_ = 0; dirty_ = true;
        return;

    case KEY_END:
        if (cursorByte_ != inputBuf_.size()) {
            cursorByte_ = inputBuf_.size();
            dirty_ = true;
        }
        return;

    case KEY_UP: {
        if (history_.empty()) return;
        if (historyIdx_ == 0) historyIdx_ = history_.size();
        if (historyIdx_ > 0) --historyIdx_;
        inputBuf_ = history_[historyIdx_];
        cursorByte_ = inputBuf_.size();
        dirty_ = true;
        return;
    }

    case KEY_DOWN: {
        if (history_.empty() || historyIdx_ == 0) return;
        ++historyIdx_;
        if (historyIdx_ >= history_.size()) {
            historyIdx_ = 0;
            inputBuf_.clear();
        } 
        else {
            inputBuf_ = history_[historyIdx_];
        }
        cursorByte_ = inputBuf_.size();
        dirty_ = true;
        return;
    }

    case KEY_PPAGE: {
        int rows = 0, cols = 0;
        getmaxyx(msgs_, rows, cols);
        (void)cols;
        scrollOffset_ += static_cast<size_t>(rows / 2);
        size_t maxOffset = lines_.size() > static_cast<size_t>(rows)
            ? lines_.size() - static_cast<size_t>(rows)
            : 0;
        scrollOffset_ = std::min(scrollOffset_, maxOffset);
        dirty_ = true;
        return;
    }

    case KEY_NPAGE: {
        int rows = 0, cols = 0;
        getmaxyx(msgs_, rows, cols);
        (void)cols;
        size_t step = static_cast<size_t>(rows / 2);
        scrollOffset_ = scrollOffset_ > step ? scrollOffset_ - step : 0;
        dirty_ = true;
        return;
    }

    case KEY_RESIZE:
        handleResize();
        return;

    default:
        if (ch >= 32 && ch < 256) {
            unsigned char b = static_cast<unsigned char>(ch);
            if (b < 0x80) {
                inputBuf_.insert(cursorByte_, 1, static_cast<char>(b));
                ++cursorByte_;
                dirty_ = true;
            } 
            else if ((b & 0xC0) == 0xC0) {
                size_t needed = utf8CharLen(b);
                std::string seq;
                seq.push_back(static_cast<char>(b));
                nodelay(input_, FALSE);
                bool ok = true;
                for (size_t i = 1; i < needed; ++i) {
                    int cont = wgetch(input_);
                    if (cont == ERR ||
                        (static_cast<unsigned char>(cont) & 0xC0) != 0x80) {
                        ok = false;
                        break;
                    }
                    seq.push_back(static_cast<char>(cont));
                }
                nodelay(input_, TRUE);
                if (ok) {
                    inputBuf_.insert(cursorByte_, seq);
                    cursorByte_ += seq.size();
                    dirty_ = true;
                }
            }
        }
        return;
    }
}

void Ui::redraw() 
{
    werase(msgs_);
    int rows = 0, cols = 0;
    getmaxyx(msgs_, rows, cols);

    size_t total      = lines_.size();
    size_t maxVisible = static_cast<size_t>(rows);
    size_t end        = total > scrollOffset_ ? total - scrollOffset_ : 0;
    size_t start      = end > maxVisible ? end - maxVisible : 0;

    for (size_t i = start; i < end; ++i)
        renderStyledLine(static_cast<int>(i - start), lines_[i], cols);
    wnoutrefresh(msgs_);

    werase(status_);
    int sr = 0, sc = 0;
    getmaxyx(status_, sr, sc);
    (void)sr;
    mvwaddnstr(status_, 0, 0, statusText_.c_str(), sc);
    wnoutrefresh(status_);

    werase(input_);
    int ir = 0, ic = 0;
    getmaxyx(input_, ir, ic);
    (void)ir;
    mvwaddstr(input_, 0, 0, "> ");
    int inputStart = 2;
    int budget = ic - inputStart;
    if (budget < 1) budget = 1;

    size_t cursorCol = utf8DisplayWidth(std::string_view(inputBuf_).substr(0, cursorByte_));
    size_t viewCol = cursorCol >= static_cast<size_t>(budget)
        ? cursorCol - static_cast<size_t>(budget) + 1
        : 0;

    size_t viewByte = 0;
    for (size_t col = 0; col < viewCol && viewByte < inputBuf_.size(); ++col) {
        viewByte += utf8CharLen(static_cast<unsigned char>(inputBuf_[viewByte]));
    }

    mvwaddnstr(input_, 0, inputStart,
               inputBuf_.c_str() + viewByte,
               static_cast<int>(inputBuf_.size() - viewByte));
    wmove(input_, 0, inputStart + static_cast<int>(cursorCol - viewCol));
    wnoutrefresh(input_);

    doupdate();
    dirty_ = false;
}

void Ui::renderStyledLine(int row, std::string_view line, int maxCols) {
    int   attrs = A_NORMAL;
    short curFg = -1;

    auto setAttr = [&]() {
        wattrset(msgs_, attrs | (curFg >= 0 ? COLOR_PAIR(curFg + 1) : 0));
    };

    setAttr();
    wmove(msgs_, row, 0);

    int col = 0;
    for (size_t i = 0; i < line.size() && col < maxCols; ) {
        unsigned char c = static_cast<unsigned char>(line[i]);
        switch (c) {
        case IRC_BOLD:
            attrs ^= A_BOLD;
            setAttr();
            ++i;
            break;
        case IRC_UNDERLINE:
            attrs ^= A_UNDERLINE;
            setAttr();
            ++i;
            break;
        case IRC_ITALIC:
#ifdef A_ITALIC
            attrs ^= A_ITALIC;
            setAttr();
#endif
            ++i;
            break;
        case 0x16:  // reverse (mIRC)
            attrs ^= A_REVERSE;
            setAttr();
            ++i;
            break;
        case IRC_RESET:
            attrs = A_NORMAL;
            curFg = -1;
            setAttr();
            ++i;
            break;
        case IRC_COLOR: {
            ++i;
            size_t pos = i;
            int fg = parseDigits(line, pos);
            if (pos < line.size() && line[pos] == ',') {
                size_t save = pos;
                ++pos;
                if (parseDigits(line, pos) < 0) pos = save;
            }
            i = pos;
            if (fg >= 0 && hasColor_) {
                curFg = static_cast<short>(fg & 15);
            } 
            else if (fg < 0) {
                curFg = -1;
            }
            setAttr();
            break;
        }
        default: {
            if (c < 32) { ++i; break; } // skip other control bytes
            size_t len = utf8CharLen(c);
            if (i + len > line.size()) len = line.size() - i;
            waddnstr(msgs_, line.data() + i, static_cast<int>(len));
            i += len;
            ++col;
            break;
        }
        }
    }
    wattrset(msgs_, A_NORMAL);
}