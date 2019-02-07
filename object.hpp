//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/object.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/object.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_OBJECT_HPP
#define ISPTECH_ORB_OBJECT_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/function.hpp"
#include "isptech/orb/object_id.hpp"
#include "isptech/orb/future.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Object Implementation
*/
class Object_impl : boost::totally_ordered<Object_impl> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Object_impl() = default;
    Object_impl(Object_id, Interface_ptr);
    Object_impl(const Object_impl&) = default;
    Object_impl& operator=(const Object_impl&) = default;
    Object_impl(Object_impl&&);
    Object_impl& operator=(Object_impl&&);
    friend void swap(Object_impl&, Object_impl&);

    // Function Invocation
    Future<bool>::Awaitable invoke(Object_id, Function, Const_buffers in, Io_buffer* outp) const;
    Future<bool>::Awaitable invoke(Object_id, Function, Const_buffers in, Mutable_buffers* outp) const;

    // Comparisons
    friend bool operator==(const Object_impl&, const Object_impl&);
    friend bool operator< (const Object_impl&, const Object_impl&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Object Interface
*/
class Object_impl::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Member Function Invocation
    virtual Future<bool>::Awaitable invoke(Object_id, Function, Const_buffer in, Io_buffer* outp) const = 0;
    virtual Future<bool>::Awaitable invoke(Object_id, Function, Const_buffer in, Mutable_buffers* outp) const = 0;
};


/*
    Object
*/
class Object : boost::totally_ordered<Object> {
public:
    // Construct/Copy/Move
    Object() = default;
    Object(Object_id, Object_impl);
    Object(const Object&) = default;
    Object& operator=(const Object&) = default;
    Object(Object&&);
    Object& operator=(Object&&);
    friend void swap(Object&, Object&);

    // Identity
    void        identity(Object_id);
    Object_id   identity() const;

    // Implementation
    void                implementation(Object_impl);
    const Object_impl&  implementation() const;

    // Function Invocation
    Future<bool>::Awaitable invoke(Function, Const_buffers in, Io_buffer* outp) const;
    Future<bool>::Awaitable invoke(Function, Const_buffers in, Mutable_buffers* outp) const;

    // Comparisons
    friend bool operator==(const Object&, const Object&);
    friend bool operator< (const Object&, const Object&);

private:
    // Data
    Object_id   object;
    Object_impl impl;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_OBJECT_HPP

//  $CUSTOM_FOOTER$
