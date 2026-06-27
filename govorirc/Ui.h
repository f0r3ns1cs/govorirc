#pragma once

#include "config.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

class Ui 
{
public:
    using LineSubmitHandler = std::function<void(std::string_view)>;

    Ui();
    ~Ui();

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    void addLine(std::string body);
    void setStatus(std::string s);

    void setLineHandler(LineSubmitHandler h) { lineHandler_ = std::move(h); }

    void tick();

    void napIfIdle(int ms) const;

    void stop() noexcept { stop_ = true; }
    bool shouldStop() const noexcept { return stop_; }

    void handleResize();

private:
    void redraw();
    void handleKey(int ch);
    void renderStyledLine(int row, std::string_view line, int maxCols);

    static std::size_t utf8CharLen(unsigned char first);
    static std::size_t utf8DisplayWidth(std::string_view s);

    WINDOW*                     msgs_ = nullptr;
    WINDOW*                     status_ = nullptr;
    WINDOW*                     input_ = nullptr;

    std::vector<std::string>    lines_;
    std::string                 statusText_;
    std::string                 inputBuf_;
    std::size_t                 cursorByte_ = 0;
    std::size_t                 scrollOffset_ = 0;
    std::vector<std::string>    history_;
    std::size_t                 historyIdx_ = 0;

    LineSubmitHandler           lineHandler_;
    bool                        dirty_ = true;
    bool                        stop_ = false;
    bool                        hasColor_ = false;
};
