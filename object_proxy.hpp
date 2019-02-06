//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/object_proxy.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/object_proxy.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_OBJECT_PROXY_HPP
#define ISPTECH_ORB_OBJECT_PROXY_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/object.hpp"
#include "isptech/coroutine/task.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Two-Way Object Proxy
*/
class Twoway_object_proxy : boost::totally_ordered<Twoway_object_proxy> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;
    using Future_bool   = Coroutine::Future<bool>;

    // Construct/Copy/Move
    Twoway_object_proxy() = default;
    Twoway_object_proxy(Object_id, Interface_ptr);
    Twoway_object_proxy(const Twoway_object_proxy&) = default;
    Twoway_object_proxy& operator=(const Twoway_object_proxy&) = default;
    Twoway_object_proxy(Twoway_object_proxy&&);
    Twoway_object_proxy& operator=(Twoway_object_proxy&&);
    friend void swap(Twoway_object_proxy&, Twoway_object_proxy&);

    // Object Identity
    Object_id object() const;

    // Member Function Invocation
    Future_bool::Awaitable invoke(const Member_function&, Const_buffers in, Io_buffer* outp) const;
    Future_bool::Awaitable invoke(const Member_function&, Const_buffers in, Mutable_buffers* outp) const;

    // Comparisons
    friend bool operator==(const Twoway_object_proxy&, const Twoway_object_proxy&);
    friend bool operator< (const Twoway_object_proxy&, const Twoway_object_proxy&);

private:
    // Data
    Object_id       oid;
    Interface_ptr   ifacep;
};


/*
    Two-Way Object Proxy Interface
*/
class Twoway_object_proxy::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Member Function Invocation
    virtual Future_bool invoke(Object_id, const Member_function&, Const_buffer in, Io_buffer* outp) const = 0;
    virtual Future_bool invoke(Object_id, const Member_function&, Const_buffer in, Mutable_buffers* outp) const = 0;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_OBJECT_PROXY_HPP

//  $CUSTOM_FOOTER$
