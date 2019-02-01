//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/buffer.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/buffer.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_BUFFER_HPP
#define ISPTECH_ORB_BUFFER_HPP

#include "boost/operators.hpp"


/*
    Information and Sensor Processing Technology Coroutine Library
*/
namespace Isptech   {
namespace Orb       {


class Mutable_buffer : boost::totally_ordered<Mutable_buffer> {
};


class Const_buffer : boost::totally_ordered<Const_buffer> {
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_BUFFER_HPP

//  $CUSTOM_FOOTER$
