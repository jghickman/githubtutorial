//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/coroutine/tcp_ip4_socket.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.9 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2019/01/18 23:06:47 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/coroutine/tcp_ip4_socket.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_COROUTINE_TCP_IP4_SOCKET_HPP
#define ISPTECH_COROUTINE_TCP_IP4_SOCKET_HPP

#include "isptech/coroutine/task.hpp"
#include "boost/operators.hpp"
#include <array>
#include <memory>
#include <winsock2.h>


/*
    Information and Sensor Processing Technology Coroutine Library
*/
namespace Isptech   {
namespace Coroutine {


/*
    Names/Types
*/
using Socket_port = u_short;


/*
    IPv4 Address
*/
class Ip4_address : boost::totally_ordered<Ip4_address> {
public:
    // Names/Types
    using Network_bytes = std::tr1::array<unsigned char, 4>;
    using Host_unsigned= std::uint_least32_t;

    // Construct
    Ip4_address();
    explicit Ip4_address(const Network_bytes&)

    // 
    static Ip4_address any();
};


/*
*/
class Ip4_endpoint {
public:
    Ip4_address address() const;
    Socket_port port() const;
};


}   // Coroutine
}   // Isptech


//#include "isptech/coroutine/tcp_ip4_socket.inl"

#endif  // ISPTECH_COROUTINE_TCP_IP4_SOCKET_HPP

//  $CUSTOM_FOOTER$
