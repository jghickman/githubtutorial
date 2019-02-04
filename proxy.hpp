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
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Twoway Proxy
*/
class Twoway_proxy : boost::totally_ordered<Twoway_proxy> {
public:
    // Names/Tytpes
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Twoway_proxy() = default;
    Twoway_proxy(Identity, Interface_ptr);
    Twoway_proxy(const Twoway_proxy&) = default;
    Twoway_proxy& operator=(const Twoway_proxy&) = default;
    Twoway_proxy(Twoway_proxy&&);
    Twoway_proxy& operator=(Twoway_proxy&&);
    friend void swap(Twoway_proxy&, Twoway_proxy&);

    // Function Invocation
    bool invoke(const Function&, Const_buffers in, Io_buffer* outp) const;
    bool invoke(const Function&, Const_buffers in, Mutable_buffers* outp) const;

    // Object Identity
    void        object(Identity);
    Identity    object() const;

    // Comparisons
    friend bool operator==(const Twoway_proxy&, const Twoway_proxy&);
    friend bool operator< (const Twoway_proxy&, const Twoway_proxy&);

private:
    // Data
    Identity        oid;
    Interface_ptr   ifacep;
};


/*
    Twoway_proxy Interface
*/
class Twoway_proxy::Interface {
public:
    // Function Invocation
    virtual bool invoke(Identity, const Function&, Const_buffer in, Io_buffer* outp) const = 0;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_PROXY_HPP

//  $CUSTOM_FOOTER$
