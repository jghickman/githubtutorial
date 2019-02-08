//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/message.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/message.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_MESSAGE_HPP
#define ISPTECH_ORB_MESSAGE_HPP

#include "isptech/orb/buffer.hpp"


/*
    Information and Sensor Processing Technology Obejct Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    NOTE:  All types comprising messages must be standard-layout or POD types.
*/


/*
    Message Type
*/
enum class Message_type : char {
    request,
    reply
};


/*
    Message Header
*/
class Message_header {
public:
    Message_type    type() const;
    Buffer_size     size() const;
};


/*
    Request Header
*/
class Request_header {
public:
    Buffer_size size() const;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_MESSAGE_HPP

//  $CUSTOM_FOOTER$
