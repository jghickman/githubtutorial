//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/proxy.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/proxy.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_PROXY_HPP
#define ISPTECH_ORB_PROXY_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/function.hpp"
#include "isptech/orb/identity.hpp"


/*
    Information and Sensor Processing Technology Coroutine Library
*/
namespace Isptech   {
namespace Orb       {


/*
    Proxy
*/
class Proxy : boost::totally_ordered<Proxy> {
public:
    // Construct
    Proxy();

    // Function Invocation
    void invoke(const Function&, Const_buffer in) const;

    // Observers
    Identity identity() const;

    // Comparisons
    friend bool operator==(const Proxy&, const Proxy&);
    friend bool operator< (const Proxy&, const Proxy&);

private:
};


class A;

class Foo {
public:
    A* make_a();
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_PROXY_HPP

//  $CUSTOM_FOOTER$
