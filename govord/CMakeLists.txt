add_executable(govord
    govord.cpp
    Server.cpp
    Numerics.cpp
    Motd.cpp
    Whowas.cpp

    ${CMAKE_SOURCE_DIR}/shared/SocketCompat.cpp
    ${CMAKE_SOURCE_DIR}/shared/Utils.cpp
    ${CMAKE_SOURCE_DIR}/shared/IrcMessage.cpp

    core/m_cap.cpp
    core/m_invite.cpp
    core/m_join.cpp
    core/m_kick.cpp
    core/m_list.cpp
    core/m_mode.cpp
    core/m_motd.cpp
    core/m_names.cpp
    core/m_nick.cpp
    core/m_notice.cpp
    core/m_part.cpp
    core/m_pass.cpp
    core/m_pingpong.cpp
    core/m_privmsg.cpp
    core/m_quit.cpp
    core/m_topic.cpp
    core/m_user.cpp
    core/m_who.cpp
    core/m_whois.cpp
    core/m_whowas.cpp
)

target_include_directories(govord PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(govord PRIVATE
    govorirc::common
    govorirc::net
    OpenSSL::SSL
    OpenSSL::Crypto
)
