add_executable(govorirc
    govorirc.cpp
    Socket.cpp
    Ui.cpp
    Cli.cpp

    ${CMAKE_SOURCE_DIR}/shared/SocketCompat.cpp
    ${CMAKE_SOURCE_DIR}/shared/Utils.cpp
    ${CMAKE_SOURCE_DIR}/shared/IrcMessage.cpp
)

target_link_libraries(govorirc PRIVATE
    govorirc::common
    govorirc::curses
    govorirc::net
    OpenSSL::SSL
    OpenSSL::Crypto
)

