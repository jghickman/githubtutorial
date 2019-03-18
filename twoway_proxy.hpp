//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/twoway_proxy.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/twoway_proxy.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_TWOWAY_PROXY_HPP
#define ISPTECH_ORB_TWOWAY_PROXY_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/twoway_proxy_impl.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {



/*
    Two-Way Proxy
*/
class Twoway_proxy : boost::totally_ordered<Twoway_proxy> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Twoway_proxy() = default;
    Twoway_proxy(Interface_ptr);
    Twoway_proxy& operator=(Twoway_proxy);
    Twoway_proxy(Twoway_proxy&&);
    friend void swap(Twoway_proxy&, Twoway_proxy&);

    // Function Invocation
    Future<bool> invoke(Object_id, Function_id, Io_buffer* iop) const;

    // Comparisons
    friend bool operator==(const Twoway_proxy&, const Twoway_proxy&);
    friend bool operator< (const Twoway_proxy&, const Twoway_proxy&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Two-Way Proxy Interface
*/
class Twoway_proxy::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Function Invocation
    virtual Future<bool> invoke(Object_id, Function_id, Io_buffer* iop) = 0;
};


/*
    Two-Way Proxy
*/
class Twoway_object_proxy : boost::totally_ordered<Twoway_object_proxy> {
public:
    // Construct/Copy/Move
    Twoway_object_proxy() = default;
    explicit Twoway_object_proxy(Twoway_object_proxy, Object_id = Object_id());
    Twoway_object_proxy& operator=(const Twoway_object_proxy&) = default;
    Twoway_object_proxy(Twoway_object_proxy&&);
    Twoway_object_proxy& operator=(Twoway_object_proxy&&);
    friend void swap(Twoway_object_proxy&, Twoway_object_proxy&);

    // Object Identity
    void        identity(Object_id) const;
    Object_id   identity() const;

    // Implementation
    void                base(Twoway_proxy);
    const Twoway_proxy& base() const;

    // Function Invocation
    Future<bool> invoke(Function_id, Io_buffer* iop) const;

    // Comparisons
    friend bool operator==(const Twoway_object_proxy&, const Twoway_object_proxy&);
    friend bool operator< (const Twoway_object_proxy&, const Twoway_object_proxy&);

private:
    // Data
    Object_id       obj;
    Twoway_proxy    impl;
};


}   // Orb
}   // Isptech

#endif  // ISPTECH_ORB_TWOWAY_PROXY_HPP

//  $CUSTOM_FOOTER$
